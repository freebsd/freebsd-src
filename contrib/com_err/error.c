/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      Högskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: error.c,v 1.13 1998/02/17 21:19:44 bg Exp $");
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <com_right.h>

const char *
com_right(struct et_list *list, long code)
{
    struct et_list *p;
    for (p = list; p; p = p->next) {
	if (code >= p->table->base && code < p->table->base + p->table->n_msgs)
	    return p->table->msgs[code - p->table->base];
    }
    return NULL;
}

struct foobar {
    struct et_list etl;
    struct error_table et;
};

void
initialize_error_table_r(struct et_list **list, 
			 const char **messages, 
			 int num_errors,
			 long base)
{
    struct et_list *et;
    struct foobar *f;
    for (et = *list; et; et = et->next)
        if (et->table->msgs == messages)
            return;
    f = malloc(sizeof(*f));
    if (f == NULL)
        return;
    et = &f->etl;
    et->table = &f->et;
    et->table->msgs = messages;
    et->table->n_msgs = num_errors;
    et->table->base = base;
    et->next = *list;
    *list = et;
}
			

void
free_error_table(struct et_list *et)
{
    while(et){
	struct et_list *p = et;
	et = et->next;
	free(p);
    }
}
