/*
 * Copyright (C) 1995 by Andrey A. Chernov, Moscow, Russia.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern short ospeed;

static struct stable {
	long speed;
	short code;
} table[] = {
	{115200,17},
	{57600, 16},
	{38400, 15},
	{19200, 14},
	{9600,  13},
	{4800,  12},
	{2400,  11},
	{1800,  10},
	{1200,  9},
	{600,   8},
	{300,   7},
	{200,   6},
	{150,   5},
	{134,   4},
	{110,   3},
	{75,    2},
	{50,    1},
	{0,     0},
	{-1,    -1}
};

void _set_ospeed(long speed)
{
	struct stable *stable;

	if (speed == 0) {
		ospeed = 0;
		return;
	}
	for (stable = table; stable->speed > 0; stable++) {
		/* nearest one, rounded down */
		if (stable->speed <= speed) {
			ospeed = stable->code;
			return;
		}
	}
	ospeed = 1;     /* 50, min and not hangup */
}

