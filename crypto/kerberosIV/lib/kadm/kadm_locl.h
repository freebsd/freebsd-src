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

/* $Id: kadm_locl.h,v 1.9 1997/05/20 18:40:44 bg Exp $ */

#include "config.h"
#include "protos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <time.h>
#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef SOCKS
#include <socks.h>
#endif

#include <roken.h>

#include <des.h>
#include <krb.h>
#include <krb_err.h>
#include <krb_db.h>
#include <kadm.h>
#include <kadm_err.h>

int vts_long __P((u_int32_t, u_char **, int));
int vals_to_stream __P((Kadm_vals *, u_char **));
int stream_to_vals __P((u_char *, Kadm_vals *, int));

int kadm_init_link __P((char n[], char i[], char r[]));
int kadm_change_pw __P((des_cblock));
int kadm_add __P((Kadm_vals *));
int kadm_mod __P((Kadm_vals *, Kadm_vals *));
int kadm_get __P((Kadm_vals *, u_char fl[4]));


