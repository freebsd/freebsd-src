/*
 * Copyright (c) 1999 - 2001 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* $Id: sia_locl.h,v 1.3 2001/09/13 01:15:34 assar Exp $ */

#ifndef __sia_locl_h__
#define __sia_locl_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <siad.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <roken.h>

#ifdef KRB5
#define SIA_KRB5
#elif defined(KRB4)
#define SIA_KRB4
#endif

#ifdef SIA_KRB5
#include <krb5.h>
#include <com_err.h>
#endif
#ifdef SIA_KRB4
#include <krb.h>
#include <krb_err.h>
#include <kadm.h>
#include <kadm_err.h>
#endif
#ifdef KRB4
#include <kafs.h>
#endif

#ifndef POSIX_GETPWNAM_R

#define getpwnam_r posix_getpwnam_r
#define getpwuid_r posix_getpwuid_r

#endif /* POSIX_GETPWNAM_R */

#ifndef DEBUG
#define SIA_DEBUG(X)
#else
#define SIA_DEBUG(X) SIALOG X
#endif

struct state{
#ifdef SIA_KRB5
    krb5_context context;
    krb5_auth_context auth_context;
#endif
    char ticket[MaxPathLen];
    int valid;
};

#endif /* __sia_locl_h__ */
