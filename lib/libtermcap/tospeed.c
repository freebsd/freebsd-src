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

#include <termios.h>
#include "termcap.h"

static struct stable {
	speed_t speed;
	short code;
} table[] = {
	{B115200,17},
	{B57600, 16},
	{B38400, 15},
	{B19200, 14},
	{B9600,  13},
	{B4800,  12},
	{B2400,  11},
	{B1800,  10},
	{B1200,  9},
	{B600,   8},
	{B300,   7},
	{B200,   6},
	{B150,   5},
	{B134,   4},
	{B110,   3},
	{B75,    2},
	{B50,    1},
	{B0,     0},
	{-1,    -1}
};

void __set_ospeed(speed_t speed)
{
	struct stable *stable;

	if (speed == B0) {
		ospeed = 0;
		return;
	}
	for (stable = table; stable->speed > B0; stable++) {
		/* nearest one, rounded down */
		if (stable->speed <= speed) {
			ospeed = stable->code;
			return;
		}
	}
	ospeed = 1;     /* 50, min and not hangup */
}

