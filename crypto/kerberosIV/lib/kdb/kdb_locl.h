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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
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

/* $Id: kdb_locl.h,v 1.9 1997/05/02 14:29:08 assar Exp $ */

#ifndef __kdb_locl_h
#define __kdb_locl_h

#include "config.h"
#include "protos.h"

#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <sys/types.h>

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/file.h>
#include <roken.h>

#include <krb.h>
#include <krb_db.h>

/* --- */

/* Globals! */

/* Utils */

int kerb_db_set_lockmode __P((int));
void kerb_db_fini __P((void));
int kerb_db_init __P((void));
int kerb_db_get_principal __P((char *name, char *, Principal *, unsigned int, int *));
int kerb_db_get_dba __P((char *, char *, Dba *, unsigned int, int *));

void delta_stat __P((DB_stat *, DB_stat *, DB_stat *));

int kerb_cache_init __P((void));
int kerb_cache_get_principal __P((char *name, char *, Principal *, unsigned int));
int kerb_cache_put_principal __P((Principal *, unsigned int));
int kerb_cache_get_dba __P((char *, char *, Dba *, unsigned int));
int kerb_cache_put_dba __P((Dba *, unsigned int));

void krb_print_principal __P((Principal *));

#endif /*  __kdb_locl_h */
