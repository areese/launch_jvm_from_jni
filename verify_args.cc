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

#include "args.h"
#include <assert.h>

// poor mans cppunit.
void testNoNmt() {
    const char * const testArgv[] = { "-Dosgi.requiredJavaVersion=1.6", //
            "-XstartOnFirstThread", //
            "-Dorg.eclipse.swt.internal.carbon.smallFonts", //
            "-XX:MaxPermSize=256m", //
            "-Xms40m", //
            "-Xmx512m", //
            "-Xdock:icon=../Resources/Eclipse.icns", //
            "-XstartOnFirstThread", //
            "-Dorg.eclipse.swt.internal.carbon.smallFonts", //
            NULL, //
            };

    int testArgc = 0;
    const char * const *cp = testArgv;
    while (NULL != *cp++) {
        ++testArgc;
    }

    JavaArgs jargs = { 0, };
    findJavaArgs(jargs, testArgv, testArgc);

    if (NULL != jargs.enableNMT) {
        printf("got nmt: %s\n", jargs.enableNMT);
    }
    assert(NULL==jargs.enableNMT);

    if (0 != jargs.stacksize) {
        printf("got stacksize: %llu\n", jargs.stacksize);
    }
    assert(0 == jargs.stacksize);
}

// poor mans cppunit.
void testMainClassArgs1() {
    const char * const testArgv[] = { "-Dosgi.requiredJavaVersion=1.6", //
            "-XstartOnFirstThread", //
            "-Dorg.eclipse.swt.internal.carbon.smallFonts", //
            "-XX:MaxPermSize=256m", //
            "-Xms40m", //
            "-Xmx512m", //
            "-Xdock:icon=../Resources/Eclipse.icns", //
            "-XstartOnFirstThread", //
            "-Dorg.eclipse.swt.internal.carbon.smallFonts", //
            "TestMain", //
            NULL //
            };

    int testArgc = 0;
    const char * const *cp = testArgv;
    while (NULL != *cp++) {
        ++testArgc;
    }

    JavaArgs jargs = { 0, };
    findJavaArgs(jargs, testArgv, testArgc);

    char **values = NULL;
    size_t numValues = 0;
    createJavaMainArgs(jargs, &values, numValues);

    assert(0 == strcmp(jargs.argv[jargs.mainClassNameIndex], "TestMain"));
    assert(9 == jargs.mainClassNameIndex);
    assert(NULL==jargs.argv[jargs.startJavaArgs]);
    assert(NULL==jargs.argv[jargs.lastArg]);
    assert(NULL==values);
    assert(0 == numValues);

}

// poor mans cppunit.
void testMainClassArgs2() {
    const char * const testArgv[] = { "-Dosgi.requiredJavaVersion=1.6", //
            "-XstartOnFirstThread", //
            "-Dorg.eclipse.swt.internal.carbon.smallFonts", //
            "-XX:MaxPermSize=256m", //
            "-Xms40m", //
            "-Xmx512m", //
            "-Xdock:icon=../Resources/Eclipse.icns", //
            "-XstartOnFirstThread", //
            "-Dorg.eclipse.swt.internal.carbon.smallFonts", //
            "TestMain", //
            "a1", //
            "-B2", //
            "-XssT$", //
            NULL };

    int testArgc = 0;
    const char * const *cp = testArgv;
    while (NULL != *cp++) {
        ++testArgc;
    }

    JavaArgs jargs = { 0, };
    findJavaArgs(jargs, testArgv, testArgc);

    char **values;
    size_t numValues = 0;
    createJavaMainArgs(jargs, &values, numValues);
    assert(0 == strcmp(jargs.argv[jargs.mainClassNameIndex], "TestMain"));
    assert(9 == jargs.mainClassNameIndex);

    assert(3 == numValues);

    assert(NULL!=jargs.argv[jargs.startJavaArgs]);
    assert(NULL==jargs.argv[jargs.lastArg]);

    assert(NULL!=values);
    for (int i = 0; i < numValues; i++) {
        assert(NULL != values[i]);
        assert(strlen(values[i]) > 0);
    }

}
void compare(char *in, const char *str, size_t len) {
    printf("%s == %s: %d\n", in, str, strncmp(in, str, len));
}

int main(int argc, char **argv) {
// for (int i=0;i<argc;i++) {
//     printf ("argv[%d]=%s\n",i, argv[i]);
// }

// JavaArgs jargs={0,};
// findJavaArgs(jargs,argv,argc);

// dumpArgs(jargs);

// JavaVMOption *vmOptions = NULL;
// int nOptions=0;
// setupVmOptions(jargs, &vmOptions, nOptions);
// dumpJavaVMOptions(vmOptions, nOptions);

// printf ("%d\n",atol(argv[1]));
// printf ("%llu\n",parseXss(argv[1]));

// compare(argv[1], NMT_ARG_STRING, NMT_ARG_LEN);
// compare(argv[1], XSS_ARG_STRING, XSS_ARG_LEN);
// compare(argv[1], XTSS_ARG_STRING, XTSS_ARG_LEN);

// printf("%s\n", parseNmt(argv[1]));

//testNoNmt();

    testMainClassArgs1();
    testMainClassArgs2();
}
