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
 *	BSDI setver.c,v 2.2 1996/04/08 19:33:04 bostic Exp
 *
 * $FreeBSD$
 */

#include "doscmd.h"

#if 1 /*XXXXX*/
int ms_version = 622;
#else
int ms_version = 410;
#endif

typedef struct setver_t {
    short		version;
    char		command[14];
    struct setver_t	*next;
} setver_t;

static setver_t *setver_root;

void
setver(char *cmd, short version)
{
    if (cmd) {
	setver_t *s = (setver_t *)malloc(sizeof(setver_t));

	strncpy(s->command, cmd, 14);
	s->version = version;
	s->next = setver_root;
	setver_root = s;
    } else {
	ms_version = version;
    }
}

short
getver(char *cmd)
{
    if (cmd) {
	setver_t *s = setver_root;

	while (s) {
	    if (strncasecmp(cmd, s->command, 14) == 0)
		return(s->version);
	    s = s->next;
	}
    }
    return(ms_version);
}
