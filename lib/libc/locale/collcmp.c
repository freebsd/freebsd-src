/*
 * Copyright (C) 1996 by Andrey A. Chernov, Moscow, Russia.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>
#include <string.h>
#include "collate.h"

int __collcmp (c1, c2)
	u_char c1, c2;
{
	static char s1[2], s2[2];

	if (c1 == c2)
		return (0);
	if (   (isascii(c1) && isascii(c2))
	    || (!isalpha(c1) && !isalpha(c2))
	   )
		return (((int)c1) - ((int)c2));
	if (isalpha(c1) && !isalpha(c2)) {
		if (isupper(c1))
			return ('A' - ((int)c2));
		else
			return ('a' - ((int)c2));
	} else if (isalpha(c2) && !isalpha(c1)) {
		if (isupper(c2))
			return (((int)c1) - 'A');
		else
			return (((int)c1) - 'a');
	}
	if (isupper(c1) && islower(c2))
		return (-1);
	else if (islower(c1) && isupper(c2))
		return (1);
	s1[0] = (char) c1;
	s2[0] = (char) c2;
	return strcoll(s1, s2);
}

