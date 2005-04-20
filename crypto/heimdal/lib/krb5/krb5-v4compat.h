/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

/* $Id: krb5-v4compat.h,v 1.2 2003/03/18 03:08:20 lha Exp $ */

#ifndef __KRB5_V4COMPAT_H__
#define __KRB5_V4COMPAT_H__

/* 
 * This file must only be included with v4 compat glue stuff in
 * heimdal sources.
 *
 * It MUST NOT be installed.
 */

#define		MAX_KTXT_LEN	1250

#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40

struct ktext {
    unsigned int length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    u_int32_t mbz;		/* zero to catch runaway strings */
};

struct credentials {
    char    service[ANAME_SZ];	/* Service name */
    char    instance[INST_SZ];	/* Instance */
    char    realm[REALM_SZ];	/* Auth domain */
    des_cblock session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    struct ktext ticket_st;	/* The ticket itself */
    int32_t    issue_date;	/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};


#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */
#ifndef NEVERDATE
#define NEVERDATE ((time_t)0x7fffffffL)
#endif

#define		KERB_ERR_NULL_KEY	10

int
_krb5_krb_time_to_life(time_t start, time_t end);

time_t
_krb5_krb_life_to_time(int start, int life_);

#define krb_time_to_life	_krb5_krb_time_to_life
#define krb_life_to_time	_krb5_krb_life_to_time

#endif /*  __KRB5_V4COMPAT_H__ */
