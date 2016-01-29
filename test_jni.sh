#!/bin/sh -x 
#echo ${LD_LIBRARY_PATH}
#echo $@
_JAVA_LAUNCHER_DEBUG=1 ./bin/jnivm $@ &
export BPID=$!
echo PID: $BPID
sleep 3
jcmd $BPID  VM.native_memory summary
kill $BPID
