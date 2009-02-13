/*
 * Copyright (c) 1995 - 2001, 2003 Kungliga Tekniska Högskolan
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

#include "kafs_locl.h"

RCSID("$Id: afskrb.c,v 1.17 2003/04/14 08:32:11 lha Exp $");

#ifdef KRB4

struct krb_kafs_data {
    const char *realm;
};

static int
get_cred(kafs_data *data, const char *name, const char *inst, 
	 const char *realm, uid_t uid, struct kafs_token *kt)
{
    CREDENTIALS c;
    KTEXT_ST tkt;
    int ret = krb_get_cred((char*)name, (char*)inst, (char*)realm, &c);
    
    if (ret) {
	ret = krb_mk_req(&tkt, (char*)name, (char*)inst, (char*)realm, 0);
	if (ret == KSUCCESS)
	    ret = krb_get_cred((char*)name, (char*)inst, (char*)realm, &c);
    }
    if (ret == 0)
	ret = _kafs_v4_to_kt(&c, uid, kt);
    return ret;
}

static int
afslog_uid_int(kafs_data *data,
	       const char *cell,
	       const char *realm_hint,
	       uid_t uid,
	       const char *homedir)
{
    int ret;
    struct kafs_token kt;
    char name[ANAME_SZ];
    char inst[INST_SZ];
    char realm[REALM_SZ];
    
    kt.ticket = NULL;

    if (cell == 0 || cell[0] == 0)
	return _kafs_afslog_all_local_cells (data, uid, homedir);

    /* Extract realm from ticket file. */
    ret = krb_get_tf_fullname(tkt_string(), name, inst, realm);
    if (ret != KSUCCESS)
	return ret;

    kt.ticket = NULL;
    ret = _kafs_get_cred(data, cell, realm_hint, realm, uid, &kt);
    
    if (ret == 0) {
	ret = kafs_settoken_rxkad(cell, &kt.ct, kt.ticket, kt.ticket_len);
	free(kt.ticket);
    }
    return ret;
}

static char *
get_realm(kafs_data *data, const char *host)
{
    char *r = krb_realmofhost(host);
    if(r != NULL)
	return strdup(r);
    else
	return NULL;
}

int
krb_afslog_uid_home(const char *cell, const char *realm_hint, uid_t uid,
		    const char *homedir)
{
    kafs_data kd;

    kd.name = "krb4";
    kd.afslog_uid = afslog_uid_int;
    kd.get_cred = get_cred;
    kd.get_realm = get_realm;
    kd.data = 0;
    return afslog_uid_int(&kd, cell, realm_hint, uid, homedir);
}

int
krb_afslog_uid(const char *cell, const char *realm_hint, uid_t uid)
{
    return krb_afslog_uid_home(cell, realm_hint, uid, NULL);
}

int
krb_afslog(const char *cell, const char *realm_hint)
{
    return krb_afslog_uid(cell, realm_hint, getuid());
}

int
krb_afslog_home(const char *cell, const char *realm_hint, const char *homedir)
{
    return krb_afslog_uid_home(cell, realm_hint, getuid(), homedir);
}

/*
 *
 */

int
krb_realm_of_cell(const char *cell, char **realm)
{
    kafs_data kd;

    kd.name = "krb4";
    kd.get_realm = get_realm;
    return _kafs_realm_of_cell(&kd, cell, realm);
}

int
kafs_settoken(const char *cell, uid_t uid, CREDENTIALS *c)
{
    struct kafs_token kt;
    int ret;

    kt.ticket = NULL;

    ret = _kafs_v4_to_kt(c, uid, &kt);
    if (ret)
	return ret;

    if (kt.ct.EndTimestamp < time(NULL)) {
	free(kt.ticket);
	return 0;
    }

    ret = kafs_settoken_rxkad(cell, &kt.ct, kt.ticket, kt.ticket_len);
    free(kt.ticket);
    return ret;
}

#endif /* KRB4 */
