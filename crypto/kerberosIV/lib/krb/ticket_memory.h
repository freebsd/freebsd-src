/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

/* ticket_memory.h - Storage for tickets in memory
 * Author: d93-jka@nada.kth.se - June 1996
 */

/* $Id: ticket_memory.h,v 1.8 1999/12/02 16:58:44 joda Exp $ */

#ifndef	TICKET_MEMORY_H
#define TICKET_MEMORY_H

#include "krb_locl.h"

#define CRED_VEC_SZ	20

typedef struct _tktmem
{
  char tmname[64];
  char pname[ANAME_SZ];	/* Principal's name */
  char pinst[INST_SZ];	/* Principal's instance */
  int last_cred_no;
  CREDENTIALS cred_vec[CRED_VEC_SZ];
  time_t kdc_diff;
} tktmem;

int newTktMem(const char *tf_name);
int freeTktMem(const char *tf_name);
tktmem *getTktMem(const char *tf_name);
void firstCred(void);
int nextCredIndex(void);
int currCredIndex(void);
int nextFreeIndex(void);

#endif /* TICKET_MEMORY_H */
