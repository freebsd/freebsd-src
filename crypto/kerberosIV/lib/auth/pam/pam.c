/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

/* This code is extremely ugly, and would probably be better off
   beeing completely rewritten */


#ifdef HAVE_CONFIG_H
#include<config.h>
RCSID("$Id: pam.c,v 1.18 1999/03/17 22:37:10 assar Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#define PAM_SM_AUTH
#define PAM_SM_SESSION
#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <netinet/in.h>
#include <krb.h>
#include <kafs.h>

static int
cleanup(pam_handle_t *pamh, void *data, int error_code)
{
    if(error_code != PAM_SUCCESS)
	dest_tkt();
    free(data);
    return PAM_SUCCESS;
}

static int
doit(pam_handle_t *pamh, char *name, char *inst, char *pwd, char *tkt)
{
    char realm[REALM_SZ];
    int ret;

    pam_set_data(pamh, "KRBTKFILE", strdup(tkt), cleanup);
    krb_set_tkt_string(tkt);
	
    krb_get_lrealm(realm, 1);
    ret = krb_verify_user(name, inst, realm, pwd, KRB_VERIFY_SECURE, NULL);
    memset(pwd, 0, strlen(pwd));
    switch(ret){
    case KSUCCESS:
	return PAM_SUCCESS;
    case KDC_PR_UNKNOWN:
	return PAM_USER_UNKNOWN;
    case SKDC_CANT:
    case SKDC_RETRY:
    case RD_AP_TIME:
	return PAM_AUTHINFO_UNAVAIL;
    default:
	return PAM_AUTH_ERR;
    }
}

static int
auth_login(pam_handle_t *pamh, int flags, char *user, struct pam_conv *conv)
{
    int ret;
    struct pam_message msg, *pmsg;
    struct pam_response *resp;
    char prompt[128];

    pmsg = &msg;
    msg.msg_style = PAM_PROMPT_ECHO_OFF;
    snprintf(prompt, sizeof(prompt), "%s's Password: ", user);
    msg.msg = prompt;

    ret = conv->conv(1, (const struct pam_message**)&pmsg, 
		     &resp, conv->appdata_ptr);
    if(ret != PAM_SUCCESS)
	return ret;
    
    {
	char tkt[1024];
	struct passwd *pw = getpwnam(user);

	if(pw){
	    snprintf(tkt, sizeof(tkt),
		     "%s%u", TKT_ROOT, (unsigned)pw->pw_uid);
	    ret = doit(pamh, user, "", resp->resp, tkt);
	    if(ret == PAM_SUCCESS)
		chown(tkt, pw->pw_uid, pw->pw_gid);
	}else
	    ret = PAM_USER_UNKNOWN;
	memset(resp->resp, 0, strlen(resp->resp));
	free(resp->resp);
	free(resp);
    }
    return ret;
}

static int
auth_su(pam_handle_t *pamh, int flags, char *user, struct pam_conv *conv)
{
    int ret;
    struct passwd *pw;
    struct pam_message msg, *pmsg;
    struct pam_response *resp;
    char prompt[128];
    krb_principal pr;
    
    pr.realm[0] = 0;
    ret = pam_get_user(pamh, &user, "login: ");
    if(ret != PAM_SUCCESS)
	return ret;
    
    pw = getpwuid(getuid());
    if(strcmp(user, "root") == 0){
	strcpy_truncate(pr.name, pw->pw_name, sizeof(pr.name));
	strcpy_truncate(pr.instance, "root", sizeof(pr.instance));
    }else{
	strcpy_truncate(pr.name, user, sizeof(pr.name));
	pr.instance[0] = 0;
    }
    pmsg = &msg;
    msg.msg_style = PAM_PROMPT_ECHO_OFF;
    snprintf(prompt, sizeof(prompt), "%s's Password: ", krb_unparse_name(&pr));
    msg.msg = prompt;

    ret = conv->conv(1, (const struct pam_message**)&pmsg, 
		     &resp, conv->appdata_ptr);
    if(ret != PAM_SUCCESS)
	return ret;
    
    {
	char tkt[1024];

	snprintf(tkt, sizeof(tkt),"%s_%s_to_%s",
		 TKT_ROOT, pw->pw_name, user);
	ret = doit(pamh, pr.name, pr.instance, resp->resp, tkt);
	if(ret == PAM_SUCCESS)
	    chown(tkt, pw->pw_uid, pw->pw_gid);
	memset(resp->resp, 0, strlen(resp->resp));
	free(resp->resp);
	free(resp);
    }
    return ret;
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    char *user;
    int ret;
    struct pam_conv *conv;
    ret = pam_get_user(pamh, &user, "login: ");
    if(ret != PAM_SUCCESS)
	return ret;

    ret = pam_get_item(pamh, PAM_CONV, (void*)&conv);
    if(ret != PAM_SUCCESS)
	return ret;

    
    if(getuid() != geteuid())
	return auth_su(pamh, flags, user, conv);
    else
	return auth_login(pamh, flags, user, conv);
}

int 
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    return PAM_SUCCESS;
}


int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    char *tkt;
    void *user;
    const char *homedir = NULL;

    if(pam_get_item (pamh, PAM_USER, &user) == PAM_SUCCESS) {
	struct passwd *pwd;

	pwd = getpwnam ((char *)user);
	if (pwd != NULL)
	    homedir = pwd->pw_dir;
    }

    pam_get_data(pamh, "KRBTKFILE", (const void**)&tkt);
    setenv("KRBTKFILE", tkt, 1);
    if(k_hasafs()){
	k_setpag();
	krb_afslog_home(0, 0, homedir);
    }
    return PAM_SUCCESS;
}


int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    dest_tkt();
    if(k_hasafs())
	k_unlog();
    return PAM_SUCCESS;
}
