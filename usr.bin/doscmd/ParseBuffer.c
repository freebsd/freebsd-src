/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI ParseBuffer.c,v 2.2 1996/04/08 19:32:15 bostic Exp
 *
 * $Id: ParseBuffer.c,v 1.2 1996/09/22 05:52:53 miff Exp $
 */

#include <stdlib.h>

int
ParseBuffer(obuf, av, mac)
char *obuf;
char **av;
int mac;
{
	static char *_buf;
	char *buf;
	static int buflen = 0;
	int len;

        register char *b = buf;
        register char *p;
        register char **a;
	register char **e;

	len = strlen(obuf) + 1;
	if (len > buflen) {
		if (buflen)
			free(_buf);
		buflen = (len + 1023) & ~1023;
		_buf = malloc(buflen);
	} 
	buf = _buf;
	strcpy(buf, obuf);

        a = av;
	e = &av[mac];

        while (*buf) {
                while (*buf == ' ' || *buf == '\t' || *buf == '\n')
                        ++buf;
                if (*buf) {
                        p = b = buf;

                        *a++ = buf;
			if (a == e) {
				a[-1] = (char *)0;
				return(mac - 1);
			}

                        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n')) {
                                *b++ = *p++ & 0177;
                        }
                        if (*p)
                                ++p;
                        *b = 0;
                        buf = p;
                }
        }
        *a = (char *)0;
        return(a - av);
}
