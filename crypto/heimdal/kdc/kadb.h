/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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

/* $Id: kadb.h,v 1.3 2000/03/03 12:36:26 assar Exp $ */

#ifndef __kadb_h__
#define __kadb_h__

#define HASHSIZE 8191

struct ka_header {
    int32_t version1;			/* file format version, should
					   match version2 */
    int32_t size;
    int32_t free_ptr;
    int32_t eof_ptr;
    int32_t kvno_ptr;
    int32_t stats[8];
    int32_t admin_accounts;
    int32_t special_keys_version;
    int32_t hashsize;			/* allocated size of hash */
    int32_t hash[HASHSIZE];
    int32_t version2;
};

struct ka_entry {
    int32_t flags;			/* see below */
    int32_t next;			/* next in hash list */
    int32_t valid_end;			/* expiration date */
    int32_t mod_time;			/* time last modified */
    int32_t mod_ptr;			/* pointer to modifier */
    int32_t pw_change;			/* last pw change */
    int32_t max_life;			/* max ticket life */
    int32_t kvno;
    int32_t foo2[2];			/* huh? */
    char name[64];
    char instance[64];
    char key[8];
    u_char pw_expire;			/* # days before password expires  */
    u_char spare;
    u_char attempts;
    u_char locktime;
};

#define KAFNORMAL	(1<<0)
#define KAFADMIN        (1<<2)	/* an administrator */
#define KAFNOTGS        (1<<3)	/* ! allow principal to get or use TGT */
#define KAFNOSEAL       (1<<5)	/* ! allow principal as server in GetTicket */
#define KAFNOCPW        (1<<6)	/* ! allow principal to change its own key */
#define KAFSPECIAL      (1<<8)	/* set if special AuthServer principal */

#define DEFAULT_DATABASE "/usr/afs/db/kaserver.DB0"

#endif /* __kadb_h__ */
