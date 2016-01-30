// Copyright 2016 Yahoo Inc.
// Licensed under the terms of the New-BSD license. Please see LICENSE file in the project root for terms.

#ifndef _args_h_
#define _args_h_

struct JavaArgs {
    // argv from main
    const char * const *argv;
    // start of the jvm args
    int startJvmArgs;
    // end of jvm args
    int endJvmArgs;
    // index into argv of mainclassname
    int mainClassNameIndex;
    // index into argv of the java args
    int startJavaArgs;
    // last argument
    int lastArg;

    // stacksize if -Xss or -XX:ThreadStackSize is found.
    uint64_t stacksize;
    // value of -XX:NativeMemoryTracking= if found
    char *enableNMT;
};

extern const char *NMT_ARG_STRING;
extern const size_t NMT_ARG_LEN;
extern const char *XSS_ARG_STRING;
extern const size_t XSS_ARG_LEN;
extern const char *XTSS_ARG_STRING;
extern const size_t XTSS_ARG_LEN;

/**
 * Given argc and argv, find:
 * JVM Args, all of the - options prior to the  * MainClass
 */
void findJavaArgs(JavaArgs &jargs, const char * const *argv, int argc);

/**
 * calloc a JavaVMOption struct and fill it out based on the values in JavaArgs which were setup by findJavaArgs
 * Also if anything is in extraOpts, set those.
 * On linux this should include java.launcher.pid
 * FIXME: add a createExtraOpts, as these are needed for jps to work.
 */
void setupVmOptions(JavaArgs &jargs, JavaVMOption **output, int &size,
        char **extraOpts, const int &nExtraOpts);

/**
 * Read the -Xss4T switch and return 4 Terabytes in bytes.
 * This is because -Xss is -Xss[0-9]+[KMGT]? format
 * This is needed because you have to set the stacksize before you create the vm
 */
uint64_t parseXss(const char *input);

/**
 * Read the -XX:NativeMemoryTracking=<detail|summary|off> and return detail|summary|off
 * This is needed because you have to set the NMT variable before you create the vm...
 * (And maybe before you dlopen it).
 */
char *parseNmt(const char *in);

/**
 * return true if in starts with str.
 * This is used to check for -Xss and -XX:NativeMemoryTracking and -XX:threadsize
 */
bool isPartialArg(const char *in, const char *str);

/**
 * Fill values with char strings from javargs
 */
void createJavaMainArgs(JavaArgs &jargs, char ***values, size_t &numValues);

/**
 */
void charStringsToJavaStringArray(JNIEnv *jniEnv, jobjectArray &jarray,
        char **in, const size_t len);

/**
 * In order for jps to work correctly, you need to pass a couple of Sun options.
 * This sets those up, as one of them is the pid.  (on linux, but we'll set it always).
 */
void setupExtraOpts(const char *javaMainClassName, char ***extraOpts,
        int &nExtraOpts);


// void dumpJavaVMArgs(JavaVMInitArgs &args);
void dumpJavaVMOptions(const JavaVMOption * const options, const int nOptions);

void dumpArgs(JavaArgs &jargs);

#endif
