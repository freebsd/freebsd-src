/*
 * Copyright (c) 1995-2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: verify.c,v 1.25 2001/06/18 13:11:33 assar Exp $");
#endif
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#ifdef KRB5
#include <krb5.h>
#endif
#ifdef KRB4
#include <krb.h>
#include <kafs.h>
#endif
#include <roken.h>

#ifdef KRB5
static char krb5ccname[128];
#endif
#ifdef KRB4
static char krbtkfile[128];
#endif

/* 
   In some cases is afs_gettktstring called twice (once before
   afs_verify and once after afs_verify).
   In some cases (rlogin with access allowed via .rhosts) 
   afs_verify is not called!
   So we can't rely on correct value in krbtkfile in some
   cases!
*/

static int correct_tkfilename=0;
static int pag_set=0;

#ifdef KRB4
static void
set_krbtkfile(uid_t uid)
{
    snprintf (krbtkfile, sizeof(krbtkfile), "%s%d", TKT_ROOT, (unsigned)uid);
    krb_set_tkt_string (krbtkfile);
    correct_tkfilename = 1;
}
#endif

/* XXX this has to be the default cache name, since the KRB5CCNAME
 * environment variable isn't exported by login/xdm
 */

#ifdef KRB5
static void
set_krb5ccname(uid_t uid)
{
    snprintf (krb5ccname, sizeof(krb5ccname), "FILE:/tmp/krb5cc_%d", uid);
#ifdef KRB4
    snprintf (krbtkfile, sizeof(krbtkfile), "%s%d", TKT_ROOT, (unsigned)uid);
#endif
    correct_tkfilename = 1;
}
#endif

static void
set_spec_krbtkfile(void)
{
    int fd;
#ifdef KRB4
    snprintf (krbtkfile, sizeof(krbtkfile), "%s_XXXXXX", TKT_ROOT);
    fd = mkstemp(krbtkfile);
    close(fd);
    unlink(krbtkfile); 
    krb_set_tkt_string (krbtkfile);
#endif
#ifdef KRB5
    snprintf(krb5ccname, sizeof(krb5ccname),"FILE:/tmp/krb5cc_XXXXXX");
    fd=mkstemp(krb5ccname+5);
    close(fd);
    unlink(krb5ccname+5);
#endif
}

#ifdef KRB5
static int
verify_krb5(struct passwd *pwd,
	    char *password,
	    int32_t *exp,
	    int quiet)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_ccache ccache;
    krb5_principal principal;
    
    ret = krb5_init_context(&context);
    if (ret) {
	syslog(LOG_AUTH|LOG_DEBUG, "krb5_init_context failed: %d", ret);
	goto out;
    }

    ret = krb5_parse_name (context, pwd->pw_name, &principal);
    if (ret) {
	syslog(LOG_AUTH|LOG_DEBUG, "krb5_parse_name: %s", 
	       krb5_get_err_text(context, ret));
	goto out;
    }

    set_krb5ccname(pwd->pw_uid);
    ret = krb5_cc_resolve(context, krb5ccname, &ccache);
    if(ret) {
	syslog(LOG_AUTH|LOG_DEBUG, "krb5_cc_resolve: %s", 
	       krb5_get_err_text(context, ret));
	goto out;
    }

    ret = krb5_verify_user_lrealm(context,
				  principal,
				  ccache,
				  password,
				  TRUE,
				  NULL);
    if(ret) {
	syslog(LOG_AUTH|LOG_DEBUG, "krb5_verify_user: %s", 
	       krb5_get_err_text(context, ret));
	goto out;
    }

    if(chown(krb5_cc_get_name(context, ccache), pwd->pw_uid, pwd->pw_gid)) {
	syslog(LOG_AUTH|LOG_DEBUG, "chown: %s", 
	       krb5_get_err_text(context, errno));
	goto out;
    }

#ifdef KRB4
    if (krb5_config_get_bool(context, NULL,
			     "libdefaults",
			     "krb4_get_tickets",
			     NULL)) {
	CREDENTIALS c;
	krb5_creds mcred, cred;
        krb5_realm realm;

        krb5_get_default_realm(context, &realm);
	krb5_make_principal(context, &mcred.server, realm,
			    "krbtgt",
			    realm,
			    NULL);
	free (realm);
	ret = krb5_cc_retrieve_cred(context, ccache, 0, &mcred, &cred);
	if(ret == 0) {
	    ret = krb524_convert_creds_kdc_ccache(context, ccache, &cred, &c);
	    if(ret)
		krb5_warn(context, ret, "converting creds");
	    else {
		set_krbtkfile(pwd->pw_uid);
		tf_setup(&c, c.pname, c.pinst); 
	    }
	    memset(&c, 0, sizeof(c));
	    krb5_free_creds_contents(context, &cred);
	} else
	    syslog(LOG_AUTH|LOG_DEBUG, "krb5_cc_retrieve_cred: %s", 
		   krb5_get_err_text(context, ret));
	    
	krb5_free_principal(context, mcred.server);
    }
    if (!pag_set && k_hasafs()) {
	k_setpag();
	pag_set = 1;
    }

    if (pag_set)
	krb5_afslog_uid_home(context, ccache, NULL, NULL, 
			     pwd->pw_uid, pwd->pw_dir);
#endif
out:
    if(ret && !quiet)
	printf ("%s\n", krb5_get_err_text (context, ret));
    return ret;
}
#endif

#ifdef KRB4
static int
verify_krb4(struct passwd *pwd,
	    char *password,
	    int32_t *exp,
	    int quiet)
{
    int ret = 1;
    char lrealm[REALM_SZ];
    
    if (krb_get_lrealm (lrealm, 1) != KFAILURE) {
	set_krbtkfile(pwd->pw_uid);
	ret = krb_verify_user (pwd->pw_name, "", lrealm, password,
			       KRB_VERIFY_SECURE, NULL);
	if (ret == KSUCCESS) {
	    if (!pag_set && k_hasafs()) {
		k_setpag ();
		pag_set = 1;
            }
            if (pag_set)
		krb_afslog_uid_home (0, 0, pwd->pw_uid, pwd->pw_dir);
	} else if (!quiet)
	    printf ("%s\n", krb_get_err_text (ret));
    }
    return ret;
}
#endif

int
afs_verify(char *name,
	   char *password,
	   int32_t *exp,
	   int quiet)
{
    int ret = 1;
    struct passwd *pwd = k_getpwnam (name);

    if(pwd == NULL)
	return 1;

    if (!pag_set && k_hasafs()) {
        k_setpag();
        pag_set=1;
    }

    if (ret)
	ret = unix_verify_user (name, password);
#ifdef KRB5
    if (ret)
	ret = verify_krb5(pwd, password, exp, quiet);
#endif
#ifdef KRB4
    if(ret)
	ret = verify_krb4(pwd, password, exp, quiet);
#endif
    return ret;
}

char *
afs_gettktstring (void)
{
    char *ptr;
    struct passwd *pwd;

    if (!correct_tkfilename) {
	ptr = getenv("LOGNAME"); 
	if (ptr != NULL && ((pwd = getpwnam(ptr)) != NULL)) {
	    set_krb5ccname(pwd->pw_uid);
#ifdef KRB4
	    set_krbtkfile(pwd->pw_uid);
	    if (!pag_set && k_hasafs()) {
                k_setpag();
                pag_set=1;
	    }
#endif
	} else {
	    set_spec_krbtkfile();
	}
    }
#ifdef KRB5
    esetenv("KRB5CCNAME",krb5ccname,1);
#endif
#ifdef KRB4
    esetenv("KRBTKFILE",krbtkfile,1);
    return krbtkfile;
#else
    return "";
#endif
}
