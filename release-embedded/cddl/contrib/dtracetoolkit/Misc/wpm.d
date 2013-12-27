#!/usr/sbin/dtrace -s
/*
 * wpm.d - Measure words per minute of typing.
 *         Written in DTrace (Solaris 10 3/05).
 *
 * $Id: wpm.d 52 2007-09-24 04:28:01Z brendan $
 *
 * USAGE:       wpm.d commandname
 *   eg,
 *		wpm.d bash
 *		wpm.d vim
 *
 * This script assumes that keystrokes arrive one at a time on STDIN. This
 * isn't the case for all processes that read keyboard input (eg, sh).
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
 * 05-Aug-2007	Brendan Gregg	Created this.
 */

#pragma D option quiet
#pragma D option switchrate=10
#pragma D option defaultargs

inline int STDIN = 0;

enum tracing_state {
	BEGIN,
	TRACING
};

dtrace:::BEGIN
/$$1 == ""/
{
	trace("USAGE: wpm.d commandname\n");
	trace("  eg,\n");
	trace("       wpm.d bash\n");
	trace("       wpm.d vim\n");
	exit(1);
}

dtrace:::BEGIN
{
	state = BEGIN;
	keys = 0;
	words = 0;
	wordsize = 0;
	countdown = 5;
	last = 0;
	printf("Measuring will start in : %2d seconds", countdown);
}

profile:::tick-1sec
/--countdown >= 0/
{
	printf("\b\b\b\b\b\b\b\b\b\b%2d seconds", countdown);
}

profile:::tick-1sec
/state == BEGIN && countdown == -1/
{
	state = TRACING;
	countdown = 60;
	printf("\nMeasuring will stop in  : %2d seconds", countdown);
}

syscall::read:entry
/state == TRACING && execname == $$1 && arg0 == STDIN/
{
	self->buf = arg1;
}

syscall::read:return
/self->buf && last/
{
	this->elapsed = (timestamp - last) / 1000000;
	@dist = quantize(this->elapsed);
	@avg = avg(this->elapsed);
	@min = min(this->elapsed);
	@max = max(this->elapsed);
}

syscall::read:return
/self->buf/
{
	keys++;
	wordsize++;
	this->key = stringof(copyin(self->buf, arg0));
	last = timestamp;
}

syscall::read:return
/self->buf && (this->key == " " || this->key == "\n" || this->key == "\r") &&
    wordsize == 1/
{
	/* recurring space */
	wordsize = 0;
	self->buf = 0;
}

syscall::read:return
/self->buf && (this->key == " " || this->key == "\n" || this->key == "\r")/
{
	words++;
	@sizes = lquantize(wordsize - 1, 0, 32, 1);
	wordsize = 0;
}

syscall::read:return
/self->buf/
{
	self->buf = 0;
}

profile:::tick-1sec
/state == TRACING && countdown == -1/
{
	printf("\n\nCharacters typed : %d\n", keys);
	printf("Words per minute : %d\n\n", words);

	printa("Minimum keystroke latency : %@d ms\n", @min);
	printa("Average keystroke latency : %@d ms\n", @avg);
	printa("Maximum keystroke latency : %@d ms\n\n", @max);

	printa("Word size distribution (letters),\n%@d\n", @sizes);
	printa("Keystroke latency distribution (ms),\n%@d\n", @dist);

	exit(0);
}
