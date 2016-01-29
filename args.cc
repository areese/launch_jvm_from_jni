// Copyright 2016 Yahoo Inc.
// Licensed under the terms of the New-BSD license. Please see LICENSE file in the project root for terms.

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/types.h>
#include <ctype.h>

#include "args.h"

#define KB 1024ULL
#define MB KB*1024ULL
#define GB MB*1024ULL
#define TB GB*1024ULL

const char *NMT_ARG_STRING = "-XX:NativeMemoryTracking=";
const size_t NMT_ARG_LEN = strlen(NMT_ARG_STRING);

const char *XSS_ARG_STRING = "-Xss";
const size_t XSS_ARG_LEN = strlen(XSS_ARG_STRING);

const char *XTSS_ARG_STRING = "-XX:ThreadStackSize=";
const size_t XTSS_ARG_LEN = strlen(XTSS_ARG_STRING);

uint64_t parseXss(const char *input) {
    // Format is -Xss[0-9][TGMK]
    int len = strlen(input);
    int i = 0;
    const char *cp = input;

    // skip flag part    
    while (cp && '\0' != cp && !isdigit(*cp)) {
        ++cp;
    }

    // cp points at the 5 in -Xss4M
    // atol will ignore the numbers after it.
    uint64_t result = atoll(cp);

    // skip value part
    while (cp && '\0' != cp && isdigit(*cp)) {
        ++cp;
    }

    if ('\0' == *cp) {
        return result;
    }
    // we have a character look at it.
    switch (*cp) {
    case 'T':
        return result * TB;

    case 'G':
        return result * GB;

    case 'M':
        return result * MB;

    case 'K':
        return result * KB;

    default:
        return result;
    }
}

bool isPartialArg(const char *in, const char *str) {
    size_t len = strlen(str);
    return 0 == strncmp(in, str, len);
}

char *parseNmt(const char *in) {
    // otherwise, we need everything after the =.
    const char *cp = in;
    while (NULL != cp && '\0' != *cp && '=' != *cp) {
        ++cp;
    }

    // skip =
    if ('\0' != *cp) {
        ++cp;
    }

    if ('\0' != *cp) {
        // we have an arg.
        return strdup(cp);
    }

    return NULL;
}

void findJavaArgs(JavaArgs &jargs, const char * const *argv, int argc) {
    bool foundMain = false;
    jargs.startJvmArgs = 1;
    jargs.lastArg = argc;
    jargs.argv = argv;
    jargs.mainClassNameIndex = argc;
    jargs.endJvmArgs = argc;

    for (int i = jargs.startJvmArgs; i < argc; i++) {
        if (!foundMain) {
            if ('-' == argv[i][0]) {
                // need to check the fun ones.
                if (isPartialArg(argv[i], XSS_ARG_STRING)
                        || isPartialArg(argv[i], XTSS_ARG_STRING)) {
                    jargs.stacksize = parseXss(argv[i]);
                } else if (isPartialArg(argv[i], NMT_ARG_STRING)) {
                    jargs.enableNMT = parseNmt(argv[i]);
                }
            } else {
                foundMain = true;
                jargs.mainClassNameIndex = i;
                jargs.endJvmArgs = i;
                jargs.startJavaArgs = i + 1;
            }
        }
    }
}

void setupVmOptions(JavaArgs &jargs, JavaVMOption **output, int &size,
        char **extraOpts, const int &nExtraOpts) {
    size = jargs.endJvmArgs - jargs.startJvmArgs;
    *output = NULL;

    JavaVMOption *dest = NULL;
    // make sure we always have an null at the end.
    if (size < 0) {
        size = 0;
    }

    size += nExtraOpts;

    dest = (JavaVMOption*) calloc(size + 1, sizeof(JavaVMOption));
    int j = 0;
    for (int i = jargs.startJvmArgs; i < jargs.endJvmArgs && j < size;
            i++, j++) {
        dest[j].optionString = strdup(jargs.argv[i]);
        dest[j].extraInfo = NULL;
    }

    for (int i = 0; i < nExtraOpts; i++) {
        dest[j].optionString = extraOpts[i];
        dest[j].extraInfo = NULL;
    }

    *output = dest;
}

// void dumpJavaVMArgs(JavaVMInitArgs &args) {
//     fprintf(stderr, "version 0x%x, ignoreUnrecognized %d, nOptions %d\n",
//         vmArgs->version, vmArgs->ignoreUnrecognized, vmArgs->nOptions);
//     for (int i = 0; i < vmArgs->nOptions; i++) {
//       fprintf(stderr, "option[%2d] = '%s', %p\n", i, vmArgs->options[i].optionString, vmArgs->options[i].extraInfo);
//     }
// }

void dumpJavaVMOptions(const JavaVMOption * const options, const int nOptions) {
    fprintf(stderr, "nOptions %d\n", nOptions);
    for (int i = 0; i < nOptions; i++) {
        fprintf(stderr, "option[%2d] = '%s', %p\n", i, options[i].optionString,
                options[i].extraInfo);
    }
}

void dumpArgs(JavaArgs &jargs) {
    fprintf(stderr, "\n");
    fprintf(stderr, "startJvmArgs=%d\n", jargs.startJvmArgs);
    fprintf(stderr, "endJvmArgs=%d\n", jargs.endJvmArgs);
    fprintf(stderr, "mainClassNameIndex=%d\n", jargs.mainClassNameIndex);
    fprintf(stderr, "startJavaArgs=%d\n", jargs.startJavaArgs);
    fprintf(stderr, "lastArg=%d\n", jargs.lastArg);

    fprintf(stderr, "jvm args\n");
    for (int i = jargs.startJvmArgs; i < jargs.endJvmArgs; i++) {
        fprintf(stderr, "argv[%d]=%s\n", i, jargs.argv[i]);
    }

    fprintf(stderr, "jvm args count=%d\n",
            jargs.endJvmArgs - jargs.startJvmArgs);

    fprintf(stderr, "main: %s\n", jargs.argv[jargs.mainClassNameIndex]);

    fprintf(stderr, "java args count=%d\n",
            jargs.lastArg - jargs.startJavaArgs);
    fprintf(stderr, "java args\n");
    for (int i = jargs.startJavaArgs; i < jargs.lastArg; i++) {
        fprintf(stderr, "argv[%d]=%s\n", i, jargs.argv[i]);
    }
}

void createJavaMainArgs(JavaArgs &jargs, char ***values, size_t &numValues) {
    *values = NULL;
    numValues = 0;
    if (jargs.lastArg <= jargs.mainClassNameIndex + 1
            || NULL == jargs.argv[jargs.startJavaArgs]) {
        return;
    }

    numValues = jargs.lastArg - jargs.startJavaArgs;

    char **ret = (char**) calloc(numValues + 1, sizeof(char*));

    for (int i = jargs.startJavaArgs, j = 0;
            i < jargs.lastArg && NULL != jargs.argv[i]; i++, j++) {
        size_t len = strlen(jargs.argv[i]);
        ret[j] = (char*) calloc(len + 1, sizeof(char));
        ret[j] = strdup(jargs.argv[i]);
    }

    *values = ret;
}

void charStringsToJavaStringArray(JNIEnv *jniEnv, jobjectArray &jarray,
        char **in, const size_t len) {
    jclass stringClass = jniEnv->FindClass("java/lang/String");
    jarray =
        jniEnv->NewObjectArray(len, stringClass, NULL);
    if (len == 0) {
        return;
    }

    for (int i = 0; i < len && NULL != in[i]; i++) {
        fprintf(stderr, "java: %s\n", in[i]);
        jstring temp = jniEnv->NewStringUTF(in[i]);
        jniEnv->SetObjectArrayElement(jarray, i, temp);
    }
}
