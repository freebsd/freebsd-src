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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: verify.c,v 1.13 1999/04/08 12:36:16 joda Exp $");
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

#if 0
static char krb5ccname[128];
#endif
static char krbtkfile[128];

#ifdef KRB4
static void
set_krbtkfile(uid_t uid)
{
    snprintf (krbtkfile, sizeof(krbtkfile), "%s%d", TKT_ROOT, (unsigned)uid);
    krb_set_tkt_string (krbtkfile);
}
#endif


#ifdef KRB5
static int
verify_krb5(struct passwd *pwd,
	    char *password,
	    int32_t *exp,
	    int quiet)
{
    krb5_context context;
    krb5_error_code ret;
    char ticket[128];
    krb5_ccache ccache;
    krb5_principal principal;
    krb5_realm realm;
    
    krb5_init_context(&context);

    krb5_get_default_realm(context, &realm);
    krb5_make_principal(context, &principal, realm, pwd->pw_name, NULL);

    if(!krb5_kuserok(context, principal, pwd->pw_name)) {
	syslog(LOG_AUTH|LOG_DEBUG, "krb5_kuserok failed");
	goto out;
    }
    /* XXX this has to be the default cache name, since the KRB5CCNAME
       environment variable isn't exported by login/xdm
       */
    snprintf(ticket, sizeof(ticket), "FILE:/tmp/krb5cc_%d", pwd->pw_uid);
    ret = krb5_cc_resolve(context, ticket, &ccache);
    if(ret) {
	syslog(LOG_AUTH|LOG_DEBUG, "krb5_cc_resolve: %s", 
	       krb5_get_err_text(context, ret));
	goto out;
    }

    ret = krb5_verify_user(context,
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
    {
	CREDENTIALS c;
	krb5_creds mcred, cred;

	krb5_make_principal(context, &mcred.server, realm,
			    "krbtgt",
			    realm,
			    NULL);
	ret = krb5_cc_retrieve_cred(context, ccache, 0, &mcred, &cred);
	if(ret == 0) {
	    ret = krb524_convert_creds_kdc(context, &cred, &c);
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
    if (k_hasafs()) {
	k_setpag();
	krb5_afslog_uid_home(context, ccache, NULL, NULL, 
			     pwd->pw_uid, pwd->pw_dir);
    }
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
	    if (k_hasafs()) {
		k_setpag ();
		krb_afslog_uid_home (0, 0, pwd->pw_uid, pwd->pw_dir);
	    }
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
#ifdef KRB5
    ret = verify_krb5(pwd, password, exp, quiet);
#endif
#ifdef KRB4
    if(ret)
	ret = verify_krb4(pwd, password, exp, quiet);
#endif
    if (ret)
	ret = unix_verify_user (name, password);
    return ret;
}

char *
afs_gettktstring (void)
{
    return krbtkfile;
}
