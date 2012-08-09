#!/usr/sbin/dtrace -CZs
/*
 * py_profile.d - sample stack traces with Python translations using DTrace.
 *                Written for the Python DTrace provider.
 *
 * $Id: py_profile.d 19 2007-09-12 07:47:59Z brendan $
 *
 * USAGE: py_profile.d { -p PID | -c cmd }	# hit Ctrl-C to end
 *
 * This samples stack traces for the process specified. This stack trace
 * will cross the Python engine and system libraries, and insert 
 * translations for Python stack frames where appropriate. This is best
 * explained with an example stack frame output,
 *
 *            libpython2.4.so.1.0`PyEval_EvalFrame+0x2fbf
 *              [ ./func_loop.py:5 (func_c) ]
 *            libpython2.4.so.1.0`fast_function+0xa8
 *            libpython2.4.so.1.0`call_function+0xda
 *            libpython2.4.so.1.0`PyEval_EvalFrame+0xbdf
 *              [ ./func_loop.py:11 (func_b) ]
 *            libpython2.4.so.1.0`fast_function+0xa8
 *            libpython2.4.so.1.0`call_function+0xda
 *            libpython2.4.so.1.0`PyEval_EvalFrame+0xbdf
 *              [ ./func_loop.py:14 (func_a) ]
 *            libpython2.4.so.1.0`fast_function+0xa8
 *            libpython2.4.so.1.0`call_function+0xda
 *            libpython2.4.so.1.0`PyEval_EvalFrame+0xbdf
 *              [ ./func_loop.py:16 (?) ]
 *
 * The lines in square brackets are the native Python frames, the rest
 * are the Python engine.
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
#define RATE	1001		/* sampling rate, Hertz */
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
