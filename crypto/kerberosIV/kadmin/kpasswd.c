/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * change your password with kerberos
 */

#include "kadm_locl.h"

RCSID("$Id: kpasswd.c,v 1.29 1999/11/13 06:33:20 assar Exp $");

static void
usage(int value)
{
    fprintf(stderr, "Usage: ");
    fprintf(stderr, "kpasswd [-h ] [-n user] [-i instance] [-r realm] ");
    fprintf(stderr, "[-u fullname]\n");
    exit(value);
}

int
main(int argc, char **argv)
{
    krb_principal principal;
    krb_principal default_principal;
    int realm_given = 0;	/* True if realm was give on cmdline */
    int use_default = 1;	/* True if we should use default name */
    int status;			/* return code */
    char pword[MAX_KPW_LEN];
    int c;
    char tktstring[MaxPathLen];
    
    set_progname (argv[0]);

    memset (&principal, 0, sizeof(principal));
    memset (&default_principal, 0, sizeof(default_principal));
    
    krb_get_default_principal (default_principal.name,
			       default_principal.instance,
			       default_principal.realm);

    while ((c = getopt(argc, argv, "u:n:i:r:h")) != -1) {
	switch (c) {
	case 'u':
	    status = krb_parse_name (optarg, &principal);
	    if (status != KSUCCESS)
		errx (2, "%s", krb_get_err_text(status));
	    if (principal.realm[0])
		realm_given++;
	    else if (krb_get_lrealm(principal.realm, 1) != KSUCCESS)
		errx (1, "Could not find default realm!");
	    break;
	case 'n':
	    if (k_isname(optarg))
		strlcpy(principal.name,
				optarg,
				sizeof(principal.name));
	    else {
		warnx("Bad name: %s", optarg);
		usage(1);
	    }
	    break;
	case 'i':
	    if (k_isinst(optarg))
		strlcpy(principal.instance,
				optarg,
				sizeof(principal.instance));
	    else {
		warnx("Bad instance: %s", optarg);
		usage(1);
	    }
	    break;
	case 'r':
	    if (k_isrealm(optarg)) {
		strlcpy(principal.realm,
				optarg,
				sizeof(principal.realm));
		realm_given++; 
	    } else {
		warnx("Bad realm: %s", optarg);
		usage(1);
	    }
	    break;
	case 'h':
	    usage(0);
	    break;
	default:
	    usage(1);
	    break;
	}
	use_default = 0;
    }
    if (optind < argc) {
	use_default = 0;
	status = krb_parse_name (argv[optind], &principal);
	if(status != KSUCCESS)
	    errx (1, "%s", krb_get_err_text (status));
    }

    if (use_default) {
	strlcpy(principal.name,
			default_principal.name,
			sizeof(principal.name));
	strlcpy(principal.instance,
			default_principal.instance,
			sizeof(principal.instance));
	strlcpy(principal.realm,
			default_principal.realm,
			sizeof(principal.realm));
    } else {
	if (!principal.name[0])
	    strlcpy(principal.name,
			    default_principal.name,
			    sizeof(principal.name));
	if (!principal.realm[0])
	    strlcpy(principal.realm,
			    default_principal.realm,
			    sizeof(principal.realm));
    }

    snprintf(tktstring, sizeof(tktstring), "%s_cpw_%u",
	     TKT_ROOT, (unsigned)getpid());
    krb_set_tkt_string(tktstring);
    
    if (get_pw_new_pwd(pword, sizeof(pword), &principal,
		       realm_given)) {
	dest_tkt ();
	exit(1);
    }
    
    status = kadm_init_link (PWSERV_NAME, KRB_MASTER, principal.realm);
    if (status != KADM_SUCCESS) 
	com_err(argv[0], status, "while initializing");
    else {
	des_cblock newkey;
	char *pw_msg; /* message from server */

	des_string_to_key(pword, &newkey);
	status = kadm_change_pw_plain((unsigned char*)&newkey, pword, &pw_msg);
	memset(newkey, 0, sizeof(newkey));
      
	if (status == KADM_INSECURE_PW)
	    warnx ("Insecure password: %s", pw_msg);
	else if (status != KADM_SUCCESS)
	    com_err(argv[0], status, " attempting to change password.");
    }
    memset(pword, 0, sizeof(pword));

    if (status != KADM_SUCCESS)
	fprintf(stderr,"Password NOT changed.\n");
    else
	printf("Password changed.\n");

    dest_tkt();
    if (status)
	return 2;
    else 
	return 0;
}
