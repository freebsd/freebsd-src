/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 *
 * $Id: strcoll.c,v 1.4 1995/04/16 22:43:45 ache Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "collate.h"

int
strcoll(s, s2)
	const char *s, *s2;
{
	int len, len2, prim, prim2, sec, sec2, ret, ret2;
	char *tt, *t, *tt2, *t2;

	if (__collate_load_error) {
		register const u_char
				*us1 = (const u_char *)s,
				*us2 = (const u_char *)s2;

		while (tolower(*us1) == tolower(*us2)) {
			if (*us1 == '\0')
				return (0);
			if (isupper(*us1) && islower(*us2))
				return (-1);
			else if (islower(*us1) && isupper(*us2))
				return (1);
			*us1++;
			*us2++;
		}
		return (tolower(*us1) - tolower(*us2));
	}

	len = len2 = 1;
	ret = ret2 = 0;
	tt = t = __collate_substitute(s);
	tt2 = t2 = __collate_substitute(s2);
	while(*t && *t2) {
		prim = prim2 = 0;
		while(*t && !prim) {
			__collate_lookup(t, &len, &prim, &sec);
			t += len;
		}
		while(*t2 && !prim2) {
			__collate_lookup(t2, &len2, &prim2, &sec2);
			t2 += len2;
		}
		if(!prim || !prim2)
			break;
		if(prim != prim2) {
			ret = prim - prim2;
			goto end;
		}
		if(!ret2)
			ret2 = sec - sec2;
	}
	if(!*t && *t2)
		ret = -(int)((u_char)*t2);
	else if(*t && !*t2)
		ret = (u_char)*t;
	else if(!*t && !*t2)
		ret = ret2;
  end:
	free(tt);
	free(tt2);

	return ret;
}
