/*
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <popper.h>
RCSID("$Id: pop_pass.c,v 1.41 2000/04/12 15:37:46 assar Exp $");

#ifdef KRB4
static int
krb4_verify_password (POP *p)
{
    int status;
    char lrealm[REALM_SZ];
    char tkt[MaxPathLen];

    status = krb_get_lrealm(lrealm,1);
    if (status == KFAILURE) {
        pop_log(p, POP_PRIORITY, "%s: (%s.%s@%s) %s", p->client,
		p->kdata.pname, p->kdata.pinst, p->kdata.prealm,
		krb_get_err_text(status));
	return 1;
    }
    snprintf(tkt, sizeof(tkt), "%s_popper.%u", TKT_ROOT, (unsigned)getpid());
    krb_set_tkt_string (tkt);

    status = krb_verify_user(p->user, "", lrealm,
			     p->pop_parm[1], KRB_VERIFY_SECURE, "pop");
    dest_tkt(); /* no point in keeping the tickets */
    return status;
}
#endif /* KRB4 */

#ifdef KRB5
static int
krb5_verify_password (POP *p)
{
    krb5_preauthtype pre_auth_types[] = {KRB5_PADATA_ENC_TIMESTAMP};
    krb5_get_init_creds_opt get_options;
    krb5_verify_init_creds_opt verify_options;
    krb5_error_code ret;
    krb5_principal client, server;
    krb5_creds creds;

    krb5_get_init_creds_opt_init (&get_options);

    krb5_get_init_creds_opt_set_preauth_list (&get_options,
					      pre_auth_types,
					      1);

    krb5_verify_init_creds_opt_init (&verify_options);
    
    ret = krb5_parse_name (p->context, p->user, &client);
    if (ret) {
	pop_log(p, POP_PRIORITY, "krb5_parse_name: %s",
		krb5_get_err_text (p->context, ret));
	return 1;
    }

    ret = krb5_get_init_creds_password (p->context,
					&creds,
					client,
					p->pop_parm[1],
					NULL,
					NULL,
					0,
					NULL,
					&get_options);
    if (ret) {
	pop_log(p, POP_PRIORITY,
		"krb5_get_init_creds_password: %s",
		krb5_get_err_text (p->context, ret));
	return 1;
    }

    ret = krb5_sname_to_principal (p->context,
				   p->myhost,
				   "pop",
				   KRB5_NT_SRV_HST,
				   &server);
    if (ret) {
	pop_log(p, POP_PRIORITY,
		"krb5_get_init_creds_password: %s",
		krb5_get_err_text (p->context, ret));
	return 1;
    }

    ret = krb5_verify_init_creds (p->context,
				  &creds,
				  server,
				  NULL,
				  NULL,
				  &verify_options);
    krb5_free_principal (p->context, client);
    krb5_free_principal (p->context, server);
    krb5_free_creds_contents (p->context, &creds);
    return ret;
}
#endif
/* 
 *  pass:   Obtain the user password from a POP client
 */

int
pop_pass (POP *p)
{
    struct passwd  *pw;
    int i;
    struct stat st;

    /* Make one string of all these parameters */
    
    for (i = 1; i < p->parm_count; ++i)
	p->pop_parm[i][strlen(p->pop_parm[i])] = ' ';

    /*  Look for the user in the password file */
    if ((pw = k_getpwnam(p->user)) == NULL)
	return (pop_msg(p,POP_FAILURE,
			"Password supplied for \"%s\" is incorrect.",
			p->user));

    if (p->kerberosp) {
#ifdef KRB4
	if (p->version == 4) {
	    if(kuserok (&p->kdata, p->user)) {
		pop_log(p, POP_PRIORITY,
			"%s: (%s.%s@%s) tried to retrieve mail for %s.",
			p->client, p->kdata.pname, p->kdata.pinst,
			p->kdata.prealm, p->user);
		return(pop_msg(p,POP_FAILURE,
			       "Popping not authorized"));
	    }
	    pop_log(p, POP_INFO, "%s: %s.%s@%s -> %s",
		    p->ipaddr,
		    p->kdata.pname, p->kdata.pinst, p->kdata.prealm,
		    p->user);
	} else
#endif /* KRB4 */
#ifdef KRB5
	if (p->version == 5) {
	    char *name;
	    
	    if (!krb5_kuserok (p->context, p->principal, p->user)) {
		pop_log (p, POP_PRIORITY,
			 "krb5 permission denied");
		return pop_msg(p, POP_FAILURE,
			       "Popping not authorized");
	    }
	    if(krb5_unparse_name (p->context, p->principal, &name) == 0) {
		pop_log(p, POP_INFO, "%s: %s -> %s",
			p->ipaddr, name, p->user);
		free (name);
	    }
	} else {
	    pop_log (p, POP_PRIORITY, "kerberos authentication failed");
	    return pop_msg (p, POP_FAILURE,
			    "kerberos authentication failed");
	}
#endif
	{ }
    } else {
	 /*  We don't accept connections from users with null passwords */
	 if (pw->pw_passwd == NULL)
	      return (pop_msg(p,
			      POP_FAILURE,
			      "Password supplied for \"%s\" is incorrect.",
			      p->user));

#ifdef OTP
	 if (otp_verify_user (&p->otp_ctx, p->pop_parm[1]) == 0)
	     /* pass OK */;
	 else
#endif
	 /*  Compare the supplied password with the password file entry */
	 if (p->auth_level != AUTH_NONE)
	     return pop_msg(p, POP_FAILURE,
			    "Password supplied for \"%s\" is incorrect.",
			    p->user);
	 else if (!strcmp(crypt(p->pop_parm[1], pw->pw_passwd), pw->pw_passwd))
	     /* pass OK */;
	 else {
	     int ret = -1;
#ifdef KRB4
	     ret = krb4_verify_password (p);
#endif
#ifdef KRB5
	     if(ret)
		 ret = krb5_verify_password (p);
#endif
	     if(ret)
		 return pop_msg(p, POP_FAILURE,
				"Password incorrect");
	 }
    }
    pop_log(p, POP_INFO, "login from %s as %s",
	    p->ipaddr, p->user);

    /*  Build the name of the user's maildrop */
    snprintf(p->drop_name, sizeof(p->drop_name), "%s/%s", POP_MAILDIR, p->user);

    if(stat(p->drop_name, &st) < 0 || !S_ISDIR(st.st_mode)){
	/*  Make a temporary copy of the user's maildrop */
	/*    and set the group and user id */
	if (pop_dropcopy(p, pw) != POP_SUCCESS) return (POP_FAILURE);
	
	/*  Get information about the maildrop */
	if (pop_dropinfo(p) != POP_SUCCESS) return(POP_FAILURE);
    } else {
	if(changeuser(p, pw) != POP_SUCCESS) return POP_FAILURE;
	if(pop_maildir_info(p) != POP_SUCCESS) return POP_FAILURE;
    }
    /*  Initialize the last-message-accessed number */
    p->last_msg = 0;

    /*  Authorization completed successfully */
    return (pop_msg (p, POP_SUCCESS,
		     "%s has %d message(s) (%ld octets).",
		     p->user, p->msg_count, p->drop_size));
}
