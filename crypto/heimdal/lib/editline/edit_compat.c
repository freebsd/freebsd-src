/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <histedit.h>

#include "edit_compat.h"

RCSID("$Id: edit_compat.c,v 1.9 2001/08/29 00:24:33 assar Exp $");

void
rl_reset_terminal(char *p)
{
}

void
rl_initialize(void)
{
}

static const char *pr;
static const char* ret_prompt(EditLine *e)
{
    return pr;
}

static History *h;

#ifdef H_SETSIZE
#define EL_INIT_FOUR 1
#else
#ifdef H_SETMAXSIZE
/* backwards compatibility */
#define H_SETSIZE H_SETMAXSIZE
#endif
#endif

char *
readline(const char* prompt)
{
    static EditLine *e;
#ifdef H_SETSIZE
    HistEvent ev;
#endif
    int count;
    const char *str;

    if(e == NULL){
#ifdef EL_INIT_FOUR
	e = el_init("", stdin, stdout, stderr);
#else
	e = el_init("", stdin, stdout);
#endif
	el_set(e, EL_PROMPT, ret_prompt);
	h = history_init();
#ifdef H_SETSIZE
	history(h, &ev, H_SETSIZE, 25);
#else
	history(h, H_EVENT, 25);
#endif
	el_set(e, EL_HIST, history, h);
	el_set(e, EL_EDITOR, "emacs"); /* XXX? */
    }
    pr = prompt ? prompt : "";
    str = el_gets(e, &count);
    if (str && count > 0) {
	char *ret = strdup (str);

	if (ret == NULL)
	    return NULL;

	if (ret[strlen(ret) - 1] == '\n')
	    ret[strlen(ret) - 1] = '\0';
	return ret;
    } 
    return NULL;
}

void
add_history(char *p)
{
#ifdef H_SETSIZE
    HistEvent ev;
    history(h, &ev, H_ENTER, p);
#else
    history(h, H_ENTER, p);
#endif
}
