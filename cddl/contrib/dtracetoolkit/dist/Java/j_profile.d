#!/usr/sbin/dtrace -CZs
/*
 * j_profile.d - sample stack traces with Java translations using DTrace.
 *
 * USAGE: j_profile.d { -p PID | -c cmd }	# hit Ctrl-C to end
 * $Id: j_profile.d 19 2007-09-12 07:47:59Z brendan $
 *
 *
 * This samples stack traces for the process specified. This stack trace
 * will cross the JVM and system libraries, and insert translations for Java
 * stack frames where appropriate. This is best explained with an example
 * stack frame output,
 *
 *            Func_loop.func_c()V
 *            Func_loop.func_b()V
 *            Func_loop.func_a()V
 *            Func_loop.main([Ljava/lang/String;)V
 *            StubRoutines (1)
 *            libjvm.so`__1cJJavaCallsLcall_helper6FpnJJavaValue_pnMmethodHan
 *            libjvm.so`__1cCosUos_exception_wrapper6FpFpnJJavaValue_pnMmetho
 *            libjvm.so`__1cJJavaCallsEcall6FpnJJavaValue_nMmethodHandle_pnRJ
 *            libjvm.so`__1cRjni_invoke_static6FpnHJNIEnv__pnJJavaValue_pnI_j
 *            libjvm.so`jni_CallStaticVoidMethod+0x15d
 *            java`JavaMain+0xd30
 *            libc.so.1`_thr_setup+0x52
 *            libc.so.1`_lwp_start
 *            101
 *
 * The lines at the top are Java frames, followed by the JVM (libjvm.so).
 * The JVM symbols may be translated by passing the output through c++filt.
 *
 * COPYRIGHT: Copyright (c) 2007 Brendan Gregg.
 *
 * CDDL HEADER START
 *
 *  The contents of this file are subject to the terms of the
 *  Common Development and Distribution License, Version 1.0 only
 *  (the "License").  You may not use this file except in compliance
 *  with the License.
 *
 *  You can obtain a copy of the license at Docs/cddl1.txt
 *  or http://www.opensolaris.org/os/licensing.
 *  See the License for the specific language governing permissions
 *  and limitations under the License.
 *
 * CDDL HEADER END
 *
 * 09-Sep-2007	Brendan Gregg	Created this.
 */

#pragma D option quiet
#pragma D option jstackstrsize=1024

/*
 * Tunables
 */
#define DEPTH	10		/* stack depth, frames */
#define RATE	101		/* sampling rate, Hertz */
#define TOP	25		/* number of stacks to output */

dtrace:::BEGIN
{
	printf("Sampling %d-level stacks at %d Hertz... Hit Ctrl-C to end.\n",
	    DEPTH, RATE);
}

profile-RATE
/pid == $target/
{
	@stacks[jstack(DEPTH)] = count();
}

dtrace:::END
{
	trunc(@stacks, TOP);
	printf("Top %d most frequently sampled stacks,\n", TOP);
	printa(@stacks);
}
