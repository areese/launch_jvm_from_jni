// Copyright 2016 Yahoo Inc.
// Licensed under the terms of the New-BSD license. Please see LICENSE file in the project root for terms.

import java.util.concurrent.TimeUnit;
import java.util.Arrays;

/**
A Simple class that lets us loop so we have enough time to run jps and jcmd in another terminal to check
**/
public class TestMain {
    public static void main(String[] args) throws InterruptedException {
        System.out.println("Argv is: "+ args.length);
        for (String s: args) {
            System.out.println(s);
        }
        for (int i=0;i<10000;i++) {
            System.err.println("!");
            TimeUnit.SECONDS.sleep(1);
        }
    }
}
