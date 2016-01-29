UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
# this is from /usr/libexec/java_home -v 1.8
#JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk1.8.0_72.jdk/Contents/Home
JAVA_HOME=`/usr/libexec/java_home -v 1.8`
JAVA_OS=darwin
endif

ifeq ($(UNAME_S),Linux)
JAVA_HOME=/usr/java/default
JAVA_OS=linux
LINUX_ADD=amd64/
endif

# include rules that can change the locations
-include custom.rules

JAVA_LIBRARY_PATH=$(JAVA_HOME)/jre/lib/$(LINUX_ADD)server/
JAVA_INCLUDES=-I$(JAVA_HOME)/include/ -I$(JAVA_HOME)/include/$(JAVA_OS)/ -L$(JAVA_LIBRARY_PATH)

#JAVA_APP_ARGS=1 2 3

JAVA_ARGS=-XX:NativeMemoryTracking=detail $(A) TestMain
CPP_ARGS=
CPP_ARGS+=-Dsun.java.launcher.diag=true
CPP_ARGS+=-Djava.class.path=.
CPP_ARGS+=-Dsun.java.launcher.diag=true
CPP_ARGS+=-XX:NativeMemoryTracking=detail
CPP_ARGS+=-Dsun.java.command=TestMain
CPP_ARGS+=-Dsun.java.launcher=SUN_STANDARD


all: compile

verify_args: bin
	g++ -g -O0 args.cc verify_args.cc $(JAVA_INCLUDES) -o bin/verify_args 

jnivm: bin
	g++ -g -O0 args.cc jnivm.cc $(JAVA_INCLUDES) -ldl -o bin/jnivm

bin:
	-mkdir bin

clean:
	rm -rf bin

compile: clean verify_args jnivm 

run_jni: compile
	echo  ./bin/jnivm $(CPP_ARGS) TestMain
	_JAVA_LAUNCHER_DEBUG=1 LD_LIBRARY_PATH=$(JAVA_LIBRARY_PATH) ./bin/jnivm $(CPP_ARGS) TestMain $(JAVA_APP_ARGS)

debug_jni: compile
	_JAVA_LAUNCHER_DEBUG=1 LD_LIBRARY_PATH=$(JAVA_LIBRARY_PATH) gdb --args ./bin/jnivm $(CPP_ARGS) TestMain $(JAVA_APP_ARGS)

run_java:
	_JAVA_LAUNCHER_DEBUG=1 java $(JAVA_ARGS)

test_jni:
	export LD_LIBRARY_PATH=$(JAVA_LIBRARY_PATH) && ./test_jni.sh $(CPP_ARGS) TestMain $(JAVA_APP_ARGS)
	
	
unit_test: verify_args
	./bin/verify_args

run_tests: unit_test test_jni
