# jnivm
This is example code for how to "correctly" start the JVM via JNI.
I had an issue where -Xss and -XX:NativeMemoryTracking=<detail|summary> wasn't working.
Digging through the launcher code and asking Oracle led to the fix which is what this code 
outlines.

Basically you need to scan for these 2 options in the command line, and if you see them
set up things before you start the jvm so it can use them.
You also must call the jvm functions via function pointers from loading the library dynamically
and not from linking with it.

https://docs.oracle.com/javase/8/docs/technotes/guides/vm/nmt-8.html
http://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/invocation.html
