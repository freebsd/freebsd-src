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

#include "krb_locl.h"

RCSID("$Id: kuserok.c,v 1.21 1997/04/01 08:18:35 joda Exp $");

#define OK 0
#define NOTOK 1
#define MAX_USERNAME 10

/* 
 * Given a Kerberos principal and a local username, determine whether
 * user is authorized to login according to the authorization file
 * ("~luser/.klogin" by default).  Returns OK if authorized, NOTOK if
 * not authorized.
 *
 * IMPORTANT CHANGE: To eliminate the need of making a distinction
 * between the 3 cases:
 *
 * 1. We can't verify that a .klogin file doesn't exist (no home dir).
 * 2. It's there but we aren't allowed to read it.
 * 3. We can read it and ~luser@LOCALREALM is (not) included.
 *
 * We instead make the assumption that luser@LOCALREALM is *always*
 * included. Thus it is impossible to have an empty .klogin file and
 * also to exclude luser@LOCALREALM from it. Root is treated differently
 * since it's home should always be available.
 *
 * OLD STRATEGY:
 * If there is no account for "luser" on the local machine, returns
 * NOTOK.  If there is no authorization file, and the given Kerberos
 * name "kdata" translates to the same name as "luser" (using
 * krb_kntoln()), returns OK.  Otherwise, if the authorization file
 * can't be accessed, returns NOTOK.  Otherwise, the file is read for
 * a matching principal name, instance, and realm.  If one is found,
 * returns OK, if none is found, returns NOTOK.
 *
 * The file entries are in the format:
 *
 *	name.instance@realm
 *
 * one entry per line.
 *
 */

int
krb_kuserok(char *name, char *instance, char *realm, char *luser)
{
    struct passwd *pwd;
    char lrealm[REALM_SZ];
    FILE *f;
    char line[1024];
    char file[MaxPathLen];
    struct stat st;

    pwd = getpwnam(luser);
    if(pwd == NULL)
	return NOTOK;
    if(krb_get_lrealm(lrealm, 1))
	return NOTOK;
    if(pwd->pw_uid != 0 &&
       strcmp(name, luser) == 0 &&
       strcmp(instance, "") == 0 &&
       strcmp(realm, lrealm) == 0)
	return OK;
    strcpy(file, pwd->pw_dir);
    strcat(file, "/.klogin");

    f = fopen(file, "r");
    if(f == NULL)
	return NOTOK;
    
    /* this is not a working test in filesystems like AFS and DFS */
    if(fstat(fileno(f), &st) < 0){
	fclose(f);
	return NOTOK;
    }
    
    if(st.st_uid != pwd->pw_uid){
	fclose(f);
	return NOTOK;
    }
    
    while(fgets(line, sizeof(line), f)){
	char fname[ANAME_SZ], finst[INST_SZ], frealm[REALM_SZ];
	if(line[strlen(line) - 1] != '\n')
	    /* read till end of line */
	    while(1){
		int c = fgetc(f);
		if(c == '\n' || c == EOF)
		    break;
	    }
	else
	    line[strlen(line) - 1] = 0;
	
	if(kname_parse(fname, finst, frealm, line))
	    continue;
	if(strcmp(name, fname))
	    continue;
	if(strcmp(instance, finst))
	    continue;
	if(frealm[0] == 0)
	    strcpy(frealm, lrealm);
	if(strcmp(realm, frealm))
	    continue;
	fclose(f);
	return OK;
    }
    fclose(f);
    return NOTOK;
}

/* compatibility interface */

int
kuserok(AUTH_DAT *auth, char *luser)
{
    return krb_kuserok(auth->pname, auth->pinst, auth->prealm, luser);
}

