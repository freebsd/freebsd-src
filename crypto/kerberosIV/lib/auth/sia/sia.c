/*
 * Copyright (c) 1995-1999 Kungliga Tekniska Högskolan
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

#include "sia_locl.h"

RCSID("$Id: sia.c,v 1.32.2.1 1999/12/20 09:49:30 joda Exp $");

int 
siad_init(void)
{
    return SIADSUCCESS;
}

int 
siad_chk_invoker(void)
{
    SIA_DEBUG(("DEBUG", "siad_chk_invoker"));
    return SIADFAIL;
}

int 
siad_ses_init(SIAENTITY *entity, int pkgind)
{
    struct state *s = malloc(sizeof(*s));
    SIA_DEBUG(("DEBUG", "siad_ses_init"));
    if(s == NULL)
	return SIADFAIL;
    memset(s, 0, sizeof(*s));
#ifdef SIA_KRB5
    krb5_init_context(&s->context);
#endif
    entity->mech[pkgind] = (int*)s;
    return SIADSUCCESS;
}

static int
setup_name(SIAENTITY *e, prompt_t *p)
{
    SIA_DEBUG(("DEBUG", "setup_name"));
    e->name = malloc(SIANAMEMIN + 1);
    if(e->name == NULL){
	SIA_DEBUG(("DEBUG", "failed to malloc %u bytes", SIANAMEMIN+1));
	return SIADFAIL;
    }
    p->prompt = (unsigned char*)"login: ";
    p->result = (unsigned char*)e->name;
    p->min_result_length = 1;
    p->max_result_length = SIANAMEMIN;
    p->control_flags = 0;
    return SIADSUCCESS;
}

static int
setup_password(SIAENTITY *e, prompt_t *p)
{
    SIA_DEBUG(("DEBUG", "setup_password"));
    e->password = malloc(SIAMXPASSWORD + 1);
    if(e->password == NULL){
	SIA_DEBUG(("DEBUG", "failed to malloc %u bytes", SIAMXPASSWORD+1));
	return SIADFAIL;
    }
    p->prompt = (unsigned char*)"Password: ";
    p->result = (unsigned char*)e->password;
    p->min_result_length = 0;
    p->max_result_length = SIAMXPASSWORD;
    p->control_flags = SIARESINVIS;
    return SIADSUCCESS;
}


static int
doauth(SIAENTITY *entity, int pkgind, char *name)
{
    struct passwd pw, *pwd;
    char pwbuf[1024];
    struct state *s = (struct state*)entity->mech[pkgind];
#ifdef SIA_KRB5
    krb5_realm *realms, *r;
    krb5_principal principal;
    krb5_ccache ccache;
    krb5_error_code ret;
#endif
#ifdef SIA_KRB4
    char realm[REALM_SZ];
    char *toname, *toinst;
    int ret;
    struct passwd fpw, *fpwd;
    char fpwbuf[1024];
    int secure;
#endif
	
    if(getpwnam_r(name, &pw, pwbuf, sizeof(pwbuf), &pwd) != 0){
	SIA_DEBUG(("DEBUG", "failed to getpwnam(%s)", name));
	return SIADFAIL;
    }

#ifdef SIA_KRB5
    ret = krb5_get_default_realms(s->context, &realms);

    for (r = realms; *r != NULL; ++r) {
	krb5_make_principal (s->context, &principal, *r, entity->name, NULL);

	if(krb5_kuserok(s->context, principal, entity->name))
	    break;
    }
    krb5_free_host_realm (s->context, realms);
    if (*r == NULL)
	return SIADFAIL;

    sprintf(s->ticket, "FILE:/tmp/krb5_cc%d_%d", pwd->pw_uid, getpid());
    ret = krb5_cc_resolve(s->context, s->ticket, &ccache);
    if(ret)
	return SIADFAIL;
#endif
	
#ifdef SIA_KRB4
    snprintf(s->ticket, sizeof(s->ticket),
	     "%s%u_%u", TKT_ROOT, (unsigned)pwd->pw_uid, (unsigned)getpid());
    krb_get_lrealm(realm, 1);
    toname = name;
    toinst = "";
    if(entity->authtype == SIA_A_SUAUTH){
	uid_t ouid;
#ifdef HAVE_SIAENTITY_OUID
	ouid = entity->ouid;
#else
	ouid = getuid();
#endif
	if(getpwuid_r(ouid, &fpw, fpwbuf, sizeof(fpwbuf), &fpwd) != 0){
	    SIA_DEBUG(("DEBUG", "failed to getpwuid(%u)", ouid));
	    return SIADFAIL;
	}
	snprintf(s->ticket, sizeof(s->ticket), "%s_%s_to_%s_%d", 
		 TKT_ROOT, fpwd->pw_name, pwd->pw_name, getpid());
	if(strcmp(pwd->pw_name, "root") == 0){
	    toname = fpwd->pw_name;
	    toinst = pwd->pw_name;
	}
    }
    if(entity->authtype == SIA_A_REAUTH) 
	snprintf(s->ticket, sizeof(s->ticket), "%s", tkt_string());
    
    krb_set_tkt_string(s->ticket);
	
    setuid(0); /* XXX fix for fix in tf_util.c */
    if(krb_kuserok(toname, toinst, realm, name)){
	SIA_DEBUG(("DEBUG", "%s.%s@%s is not allowed to login as %s", 
		   toname, toinst, realm, name));
	return SIADFAIL;
    }
#endif
#ifdef SIA_KRB5
    ret = krb5_verify_user_lrealm(s->context, principal, ccache,
				  entity->password, 1, NULL);
    if(ret){
	/* if this is most likely a local user (such as
	   root), just silently return failure when the
	   principal doesn't exist */
	if(ret != KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN && 
	   ret != KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN)
	    SIALOG("WARNING", "krb5_verify_user(%s): %s", 
		   entity->name, error_message(ret));
	return SIADFAIL;
    }
#endif
#ifdef SIA_KRB4
    if (getuid () == 0)
	secure = KRB_VERIFY_SECURE;
    else
	secure = KRB_VERIFY_NOT_SECURE;
	
    ret = krb_verify_user(toname, toinst, realm,
			  entity->password, secure, NULL);
    if(ret){
	SIA_DEBUG(("DEBUG", "krb_verify_user: %s", krb_get_err_text(ret)));
	if(ret != KDC_PR_UNKNOWN)
	    /* since this is most likely a local user (such as
	       root), just silently return failure when the
	       principal doesn't exist */
	    SIALOG("WARNING", "krb_verify_user(%s.%s): %s", 
		   toname, toinst, krb_get_err_text(ret));
	return SIADFAIL;
    }
#endif
    if(sia_make_entity_pwd(pwd, entity) == SIAFAIL)
	return SIADFAIL;
    s->valid = 1;
    return SIADSUCCESS;
}


static int 
common_auth(sia_collect_func_t *collect, 
	    SIAENTITY *entity, 
	    int siastat,
	    int pkgind)
{
    prompt_t prompts[2], *pr;
    char *name;

    SIA_DEBUG(("DEBUG", "common_auth"));
    if((siastat == SIADSUCCESS) && (geteuid() == 0))
	return SIADSUCCESS;
    if(entity == NULL) {
	SIA_DEBUG(("DEBUG", "entity == NULL"));
	return SIADFAIL | SIADSTOP;
    }
    name = entity->name;
    if(entity->acctname)
	name = entity->acctname;
    
    if((collect != NULL) && entity->colinput) {
	int num;
	pr = prompts;
	if(name == NULL){
	    if(setup_name(entity, pr) != SIADSUCCESS)
		return SIADFAIL;
	    pr++;
	}
	if(entity->password == NULL){
	    if(setup_password(entity, pr) != SIADSUCCESS)
		return SIADFAIL;
	    pr++;
	}
	num = pr - prompts;
	if(num == 1){
	    if((*collect)(240, SIAONELINER, (unsigned char*)"", num, 
			  prompts) != SIACOLSUCCESS){
		SIA_DEBUG(("DEBUG", "collect failed"));
		return SIADFAIL | SIADSTOP;
	    }
	} else if(num > 0){
	    if((*collect)(0, SIAFORM, (unsigned char*)"", num, 
			  prompts) != SIACOLSUCCESS){
		SIA_DEBUG(("DEBUG", "collect failed"));
		return SIADFAIL | SIADSTOP;
	    }
	}
    }
    if(name == NULL)
	name = entity->name;
    if(name == NULL || name[0] == '\0'){
	SIA_DEBUG(("DEBUG", "name is null"));
	return SIADFAIL;
    }

    if(entity->password == NULL || strlen(entity->password) > SIAMXPASSWORD){
	SIA_DEBUG(("DEBUG", "entity->password is null"));
	return SIADFAIL;
    }
    
    return doauth(entity, pkgind, name);
}


int 
siad_ses_authent(sia_collect_func_t *collect, 
		 SIAENTITY *entity, 
		 int siastat,
		 int pkgind)
{
    SIA_DEBUG(("DEBUG", "siad_ses_authent"));
    return common_auth(collect, entity, siastat, pkgind);
}

int 
siad_ses_estab(sia_collect_func_t *collect, 
	       SIAENTITY *entity, int pkgind)
{
    SIA_DEBUG(("DEBUG", "siad_ses_estab"));
    return SIADFAIL;
}

int 
siad_ses_launch(sia_collect_func_t *collect,
		SIAENTITY *entity,
		int pkgind)
{
    static char env[MaxPathLen];
    struct state *s = (struct state*)entity->mech[pkgind];
    SIA_DEBUG(("DEBUG", "siad_ses_launch"));
    if(s->valid){
#ifdef SIA_KRB5
	chown(s->ticket + sizeof("FILE:") - 1, 
	      entity->pwd->pw_uid, 
	      entity->pwd->pw_gid);
	snprintf(env, sizeof(env), "KRB5CCNAME=%s", s->ticket);
#endif
#ifdef SIA_KRB4
	chown(s->ticket, entity->pwd->pw_uid, entity->pwd->pw_gid);
	snprintf(env, sizeof(env), "KRBTKFILE=%s", s->ticket);
#endif
	putenv(env);
    }
#ifdef KRB4
    if (k_hasafs()) {
	char cell[64];
	k_setpag();
	if(k_afs_cell_of_file(entity->pwd->pw_dir, cell, sizeof(cell)) == 0)
	    krb_afslog(cell, 0);
	krb_afslog_home(0, 0, entity->pwd->pw_dir);
    }
#endif
    return SIADSUCCESS;
}

int 
siad_ses_release(SIAENTITY *entity, int pkgind)
{
    SIA_DEBUG(("DEBUG", "siad_ses_release"));
    if(entity->mech[pkgind]){
#ifdef SIA_KRB5
	struct state *s = (struct state*)entity->mech[pkgind];
	krb5_free_context(s->context);
#endif
	free(entity->mech[pkgind]);
    }
    return SIADSUCCESS;
}

int 
siad_ses_suauthent(sia_collect_func_t *collect,
		   SIAENTITY *entity,
		   int siastat,
		   int pkgind)
{
    SIA_DEBUG(("DEBUG", "siad_ses_suauth"));
    if(geteuid() != 0)
	return SIADFAIL;
    if(entity->name == NULL)
	return SIADFAIL;
    if(entity->name[0] == '\0') {
	free(entity->name);
	entity->name = strdup("root");
	if (entity->name == NULL)
	    return SIADFAIL;
    }
    return common_auth(collect, entity, siastat, pkgind);
}

int
siad_ses_reauthent (sia_collect_func_t *collect,
		    SIAENTITY *entity,
		    int siastat,
		    int pkgind)
{
    int ret;
    SIA_DEBUG(("DEBUG", "siad_ses_reauthent"));
    if(entity == NULL || entity->name == NULL)
	return SIADFAIL;
    ret = common_auth(collect, entity, siastat, pkgind);
    if((ret & SIADSUCCESS)){
	/* launch isn't (always?) called when doing reauth, so we must
           duplicate some code here... */
	struct state *s = (struct state*)entity->mech[pkgind];
	chown(s->ticket, entity->pwd->pw_uid, entity->pwd->pw_gid);
#ifdef KRB4
	if(k_hasafs()) {
	    char cell[64];
	    if(k_afs_cell_of_file(entity->pwd->pw_dir, 
				  cell, sizeof(cell)) == 0)
		krb_afslog(cell, 0);
	    krb_afslog_home(0, 0, entity->pwd->pw_dir);
	}
#endif
    }
    return ret;
}

int
siad_chg_finger (sia_collect_func_t *collect,
		     const char *username, 
		     int argc, 
		     char *argv[])
{
    SIA_DEBUG(("DEBUG", "siad_chg_finger"));
    return SIADFAIL;
}

#ifdef SIA_KRB5
int
siad_chg_password (sia_collect_func_t *collect,
		     const char *username, 
		     int argc, 
		     char *argv[])
{
    return SIADFAIL;
}
#endif

#ifdef SIA_KRB4
static void
sia_message(sia_collect_func_t *collect, int rendition, 
	    const char *title, const char *message)
{
    prompt_t prompt;
    prompt.prompt = (unsigned char*)message;
    (*collect)(0, rendition, (unsigned char*)title, 1, &prompt);
}

static int
init_change(sia_collect_func_t *collect, krb_principal *princ)
{
    prompt_t prompt;
    char old_pw[MAX_KPW_LEN+1];
    char *msg;
    char tktstring[128];
    int ret;
    
    SIA_DEBUG(("DEBUG", "init_change"));
    prompt.prompt = (unsigned char*)"Old password: ";
    prompt.result = (unsigned char*)old_pw;
    prompt.min_result_length = 0;
    prompt.max_result_length = sizeof(old_pw) - 1;
    prompt.control_flags = SIARESINVIS;
    asprintf(&msg, "Changing password for %s", krb_unparse_name(princ));
    if(msg == NULL){
	SIA_DEBUG(("DEBUG", "out of memory"));
	return SIADFAIL;
    }
    ret = (*collect)(60, SIAONELINER, (unsigned char*)msg, 1, &prompt);
    free(msg);
    SIA_DEBUG(("DEBUG", "ret = %d", ret));
    if(ret != SIACOLSUCCESS)
	return SIADFAIL;
    snprintf(tktstring, sizeof(tktstring), 
	     "%s_cpw_%u", TKT_ROOT, (unsigned)getpid());
    krb_set_tkt_string(tktstring);
    
    ret = krb_get_pw_in_tkt(princ->name, princ->instance, princ->realm, 
			    PWSERV_NAME, KADM_SINST, 1, old_pw);
    if (ret != KSUCCESS) {
	SIA_DEBUG(("DEBUG", "krb_get_pw_in_tkt: %s", krb_get_err_text(ret)));
	if (ret == INTK_BADPW)
	    sia_message(collect, SIAWARNING, "", "Incorrect old password.");
	else
	    sia_message(collect, SIAWARNING, "", "Kerberos error.");
	memset(old_pw, 0, sizeof(old_pw));
	return SIADFAIL;
    }
    if(chown(tktstring, getuid(), -1) < 0){
	dest_tkt();
	return SIADFAIL;
    }
    memset(old_pw, 0, sizeof(old_pw));
    return SIADSUCCESS;
}

int
siad_chg_password (sia_collect_func_t *collect,
		   const char *username, 
		   int argc, 
		   char *argv[])
{
    prompt_t prompts[2];
    krb_principal princ;
    int ret;
    char new_pw1[MAX_KPW_LEN+1];
    char new_pw2[MAX_KPW_LEN+1];
    static struct et_list *et_list;

    set_progname(argv[0]);

    SIA_DEBUG(("DEBUG", "siad_chg_password"));
    if(collect == NULL)
	return SIADFAIL;

    if(username == NULL)
	username = getlogin();

    ret = krb_parse_name(username, &princ);
    if(ret)
	return SIADFAIL;
    if(princ.realm[0] == '\0')
	krb_get_lrealm(princ.realm, 1);

    if(et_list == NULL) {
	initialize_kadm_error_table_r(&et_list);
	initialize_krb_error_table_r(&et_list);
    }

    ret = init_change(collect, &princ);
    if(ret != SIADSUCCESS)
	return ret;

again:
    prompts[0].prompt = (unsigned char*)"New password: ";
    prompts[0].result = (unsigned char*)new_pw1;
    prompts[0].min_result_length = MIN_KPW_LEN;
    prompts[0].max_result_length = sizeof(new_pw1) - 1;
    prompts[0].control_flags = SIARESINVIS;
    prompts[1].prompt = (unsigned char*)"Verify new password: ";
    prompts[1].result = (unsigned char*)new_pw2;
    prompts[1].min_result_length = MIN_KPW_LEN;
    prompts[1].max_result_length = sizeof(new_pw2) - 1;
    prompts[1].control_flags = SIARESINVIS;
    if((*collect)(120, SIAFORM, (unsigned char*)"", 2, prompts) != 
       SIACOLSUCCESS) {
	dest_tkt();
	return SIADFAIL;
    }
    if(strcmp(new_pw1, new_pw2) != 0){
	sia_message(collect, SIAWARNING, "", "Password mismatch.");
	goto again;
    }
    ret = kadm_check_pw(new_pw1);
    if(ret) {
	sia_message(collect, SIAWARNING, "", com_right(et_list, ret));
	goto again;
    }
    
    memset(new_pw2, 0, sizeof(new_pw2));
    ret = kadm_init_link (PWSERV_NAME, KRB_MASTER, princ.realm);
    if (ret != KADM_SUCCESS)
	sia_message(collect, SIAWARNING, "Error initing kadmin connection", 
		    com_right(et_list, ret));
    else {
	des_cblock newkey;
	char *pw_msg; /* message from server */

	des_string_to_key(new_pw1, &newkey);
	ret = kadm_change_pw_plain((unsigned char*)&newkey, new_pw1, &pw_msg);
	memset(newkey, 0, sizeof(newkey));
      
	if (ret == KADM_INSECURE_PW)
	    sia_message(collect, SIAWARNING, "Insecure password", pw_msg);
	else if (ret != KADM_SUCCESS)
	    sia_message(collect, SIAWARNING, "Error changing password", 
			com_right(et_list, ret));
    }
    memset(new_pw1, 0, sizeof(new_pw1));

    if (ret != KADM_SUCCESS)
	sia_message(collect, SIAWARNING, "", "Password NOT changed.");
    else
	sia_message(collect, SIAINFO, "", "Password changed.");
    
    dest_tkt();
    if(ret)
	return SIADFAIL;
    return SIADSUCCESS;
}
#endif

int
siad_chg_shell (sia_collect_func_t *collect,
		     const char *username, 
		     int argc, 
		     char *argv[])
{
    return SIADFAIL;
}

int
siad_getpwent(struct passwd *result, 
	      char *buf, 
	      int bufsize, 
	      struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_getpwuid (uid_t uid, 
	       struct passwd *result, 
	       char *buf, 
	       int bufsize, 
	       struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_getpwnam (const char *name, 
	       struct passwd *result, 
	       char *buf, 
	       int bufsize, 
	       struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_setpwent (struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_endpwent (struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_getgrent(struct group *result, 
	      char *buf, 
	      int bufsize, 
	      struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_getgrgid (gid_t gid, 
	       struct group *result, 
	       char *buf, 
	       int bufsize, 
	       struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_getgrnam (const char *name, 
	       struct group *result, 
	       char *buf, 
	       int bufsize, 
	       struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_setgrent (struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_endgrent (struct sia_context *context)
{
    return SIADFAIL;
}

int
siad_chk_user (const char *logname, int checkflag)
{
    if(checkflag != CHGPASSWD)
	return SIADFAIL;
    return SIADSUCCESS;
}
