// Copyright 2016 Yahoo Inc.
// Licensed under the terms of the New-BSD license. Please see LICENSE file in the project root for terms.

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>

#include "args.h"

/**
 * It's too bad this doesn't exist in jni.h any more, as we want the default stack size for the platform.
 * What really matters is that the alignment of nativeStackSize is correct, and that the size is correct.
 * Total length needs to be 96 bytes.
 * version is at 0
 * javaStacksize needs to be at 20
 * nativeStacksize needs to be at 24
 * Would be interesting to see what is actually filled out, but this  is all we care about.
 */
typedef struct DummyJdk11Args {
    jint version;
    uint8_t bytes1[16];
    jint nativeStackSize;
    jint javaStackSize;
    uint8_t bytes2[68];
} DummyJdk11Args;

/**
 * We can't link against the jvm (-ljvm), because if you do that, it can't enable NMT.
 * So we need to have function pointers that we can use to call the functions we need
 * after dlopening it.
 */

// Function pointer to create the JVM
typedef jint (JNICALL *CreateJavaVM_t)(JavaVM **pvm, void **env, void *args);
// Function pointer to get the default args.  Namely default stacksize
typedef jint (JNICALL *GetDefaultJavaVMInitArgs_t)(void *args);

// Probably gets the vm's that have been created?
// This function is interesting, because if you spend time in the launcher
// there is super cool code that does locking so there is only one jvm.
// they check compare and exchange 8 bytes.
//typedef jint (JNICALL *GetCreatedJavaVMs_t)(JavaVM **vmBuf, jsize bufLen,
//        jsize *nVMs);

/**
 * Handy struct for holding the function pointers we get from dlopening the jvm.
 */
struct JVMFuncs {
    CreateJavaVM_t CreateJavaVM;
    GetDefaultJavaVMInitArgs_t GetDefaultJavaVMInitArgs;
};

// silly sentinel to make sure I didn't pass something bogus.
#define SENTINEL 0XDADE100C

/**
 * This struct contains all of the args that are passed from the main thread to the new thread
 * that spawns the jvm.
 */
struct ThreadArgs {
    long sentinel;
    JVMFuncs fnt;
    JavaVMInitArgs *vmArgs;
    char *javaClassName;
    char **javaArgs;
    size_t numJavaArgs;
};

/**
 * This is the function passed to pthread_create
 * args is actually a pointer to struct ThreadArgs
 */
int continuation(void* args);

bool trace = false;

const char* NMT_Env_Name = "NMT_LEVEL_";
const int TOTAL_LEN = strlen(NMT_Env_Name) + 20;

char *getNmtEnv() {
    char *nmtEnv = (char*) calloc(TOTAL_LEN, 1);
    snprintf(nmtEnv, TOTAL_LEN, "%s%d", NMT_Env_Name, getpid());

    return nmtEnv;
}

void setNMTEnv(const char *nmtflagValue) {
    if (NULL == nmtflagValue) {
        return;
    }

    //http://hg.openjdk.java.net/jdk9/dev/jdk/rev/dde9f5cfde5f
    //http://cr.openjdk.java.net/~kshoop/8124977/webrev.00/src/java.base/share/native/libjli/jli_util.h.frames.html

    char *nmtEnv = getNmtEnv();
    if (trace) {
        fprintf(stderr, "setenv %s=%s\n", nmtEnv, nmtflagValue);
    }

    setenv(nmtEnv, strdup(nmtflagValue), 1);

    if (trace) {
        fprintf(stderr, "%s=%s\n", nmtEnv, getenv(nmtEnv));
    }

    free(nmtEnv);
}

int createVm(ThreadArgs *threadArgs) {
    // Create the JVM
    JavaVM *javaVM = NULL;
    JNIEnv *jniEnv = NULL;
    JavaVMInitArgs *vmArgs = threadArgs->vmArgs;
    JVMFuncs &fnt = threadArgs->fnt;

    if (trace) {
        fprintf(stderr, "version 0x%x, ignoreUnrecognized %d, nOptions %d\n",
                vmArgs->version, vmArgs->ignoreUnrecognized, vmArgs->nOptions);
        for (int i = 0; i < vmArgs->nOptions; i++) {
            fprintf(stderr, "option[%2d] = '%s', %p\n", i,
                    vmArgs->options[i].optionString,
                    vmArgs->options[i].extraInfo);
        }
    }

    /// create the vm.
    long flag = (fnt.CreateJavaVM)(&javaVM, (void**) &jniEnv, vmArgs);
    if (0 != flag) {
        fprintf(stderr, "Error %ld creating VM. Exiting...\n", flag);
        return -1;
    }

    if (NULL == jniEnv) {
        fprintf(stderr, "Error creating VM, jniEnv is NULL. Exiting...\n");
        return -1;
    }

    // we need the string class
    jclass stringClass = jniEnv->FindClass("java/lang/String");
    if (NULL == stringClass) {
        fprintf(stderr, "Error finding class java.lang.String.  Exiting....\n");
        jniEnv->ExceptionDescribe();
        javaVM->DestroyJavaVM();
        return 1;
    }

    // Find the class we are supposed to load.
    jclass jcls = jniEnv->FindClass(threadArgs->javaClassName);
    if (NULL == jcls) {
        fprintf(stderr, "Error finding class %s.  Exiting....\n",
                threadArgs->javaClassName);
        jniEnv->ExceptionDescribe();
        javaVM->DestroyJavaVM();
        return 1;
    }

    // we should copy the args in beyond this, but too bad for now.
    // Find the main method
    if (NULL != jcls) {
        jmethodID methodId = jniEnv->GetStaticMethodID(jcls, "main",
                "([Ljava/lang/String;)V");
        if (methodId != NULL) {
            jobjectArray javaArgVArray;
            size_t argsLen;

            charStringsToJavaStringArray(jniEnv, javaArgVArray,
                    threadArgs->javaArgs, threadArgs->numJavaArgs);
            // really should have checked exceptions here....

            jniEnv->CallStaticVoidMethod(jcls, methodId, javaArgVArray);
            if (jniEnv->ExceptionCheck()) {
                jniEnv->ExceptionDescribe();
                jniEnv->ExceptionClear();
            }
        }
    }

    // FIXME: I think this is missing teardown as NMT doesn't seem to print on exit, but jcmd now works.
    // FIXME: It seems like we probably should detach here as well.  Maybe that's the problem?
    javaVM->DestroyJavaVM();
    return 0;
}

long getDefaultStackSize(JVMFuncs &fnt, long stackSize) {
    // we need to setup vm args correctly in order to get the stack size.
    // see http://hg.openjdk.java.net/jdk8u/jdk8u-dev/jdk/file/c51a8a8ac919/src/share/bin/java.c#l1983
    // Even worse, this structure was removed from jni.h, so we have a dummy one that just matches the fields they expect.
    if (stackSize > 0) {
        return stackSize;
    }

    DummyJdk11Args args;
    args.version = JNI_VERSION_1_1;
    // Ignore return value version 1.1 isn't supported, yet it sets the stacksize...
    fnt.GetDefaultJavaVMInitArgs(&args);
    if (args.javaStackSize > 0) {
        return args.javaStackSize;
    }
    return stackSize;
}

int continuation(void* args) {
    ThreadArgs *threadArgs = (ThreadArgs*) args;
    if (SENTINEL != threadArgs->sentinel) {
        // silly safety check.
        abort();
    }

    return createVm(threadArgs);
}

int continueInNewThread(ThreadArgs *threadArgs) {
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // not sure why we have to do this.
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // FIXME: We actually need to parse the args for -Xss, and set this.
    long stackSize = 1048576;
    stackSize = getDefaultStackSize(threadArgs->fnt, stackSize);
    pthread_attr_setstacksize(&attr, stackSize);

    if (pthread_create(&tid, &attr, (void *(*)(void*))continuation, (void*)threadArgs) == 0) {
        // not really clear what would be stashed in here by the jvm.
        // this pointer is passed to pthread_exit
        void *exitValue;
        pthread_join(tid, &exitValue);
        if (trace) {
            fprintf(stderr, "result: %p\n", exitValue);
        }

        return 0;
    } else {
        return -1;
    }
}

int setupVMFuncs(JVMFuncs &fnt) {
    void *libjvm;
#ifdef __linux__
    const char *jvmpath="libjvm.so";
#else
    const char *jvmpath = "libjvm.dylib";
#endif

    if (trace) {
        fprintf(stderr, "JVM path is %s\n", jvmpath);
    }
    libjvm = dlopen(jvmpath, RTLD_NOW + RTLD_GLOBAL);

    if (NULL == libjvm) {
        return -1;
    }

    fnt.CreateJavaVM = (CreateJavaVM_t) dlsym(libjvm, "JNI_CreateJavaVM");
    if (trace) {
        fprintf(stderr, "CreateJavaVM is %p\n", fnt.CreateJavaVM);
    }

    if (NULL == fnt.CreateJavaVM) {
        return -2;
    }

    fnt.GetDefaultJavaVMInitArgs = (GetDefaultJavaVMInitArgs_t) dlsym(libjvm,
            "JNI_GetDefaultJavaVMInitArgs");
    if (trace) {
        fprintf(stderr, "GetDefaultJavaVMInitArgs is %p\n",
                fnt.GetDefaultJavaVMInitArgs);
    }

    if (NULL == fnt.GetDefaultJavaVMInitArgs) {
        return -2;
    }

//    fnt.GetCreatedJavaVMs = (GetCreatedJavaVMs_t) dlsym(libjvm,
//            "JNI_GetCreatedJavaVMs");
//    if (trace) {
//        fprintf(stderr, "GetCreatedJavaVMs is %p\n", fnt.GetCreatedJavaVMs);
//    }
//
//    if (NULL == fnt.GetCreatedJavaVMs) {
//        return -3;
//    }

    return 0;
}

int main(int argc, char **argv) {

    if (trace) {
        for (int i = 0; i < argc; i++) {
            fprintf(stderr, "arg[%2d] = '%s'\n", i, argv[i]);
        }
    }

    //collect the java arguments and parse them
    JavaArgs jargs = { 0, };
    findJavaArgs(jargs, argv, argc);
    dumpArgs(jargs);

    // Setup NMT env.
    setNMTEnv(jargs.enableNMT);

    ThreadArgs *threadArgs = (ThreadArgs*) calloc(1, sizeof(ThreadArgs));
    threadArgs->sentinel = SENTINEL;

    // Setup the vm functions
    setupVMFuncs(threadArgs->fnt);

    threadArgs->javaClassName = strdup(jargs.argv[jargs.mainClassNameIndex]);

    JavaVMOption *jvmopt;

    int nOptions = 0;
    int nExtraOpts = 0;
    char **extraOpts = NULL;

    // setup the special sun opts into extraOpts
    setupExtraOpts(jargs.argv[jargs.mainClassNameIndex], &extraOpts,
            nExtraOpts);

    // setup the jvmopt struct for the JNI_CreateJavaVM call.
    setupVmOptions(jargs, &jvmopt, nOptions, extraOpts, nExtraOpts);

    dumpJavaVMOptions(jvmopt, nOptions);

    JavaVMInitArgs *vmArgs = (JavaVMInitArgs *) calloc(1,
            sizeof(JavaVMInitArgs));
    vmArgs->version = JNI_VERSION_1_2;
    vmArgs->nOptions = nOptions;
    vmArgs->options = jvmopt;
    vmArgs->ignoreUnrecognized = JNI_FALSE;

    threadArgs->vmArgs = vmArgs;

    createJavaMainArgs(jargs, &threadArgs->javaArgs, threadArgs->numJavaArgs);

    if (trace) {
        char *nmtEnv = getNmtEnv();
        fprintf(stderr, "pid: %d\n", getpid());
        fprintf(stderr, "env: %s=%s\n", nmtEnv, getenv(nmtEnv));
        for (int i = 0; i < vmArgs->nOptions; i++) {
            fprintf(stderr, "options[%2d] = '%s'\n", i, argv[i]);
        }
        free(nmtEnv);
    }

    continueInNewThread(threadArgs);
}
