/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * Copyright.MIT.
 *
 * change your password with kerberos
 */

#ifndef	lint
#if 0
static char rcsid_kpasswd_c[] =
    "BonesHeader: /afs/athena.mit.edu/astaff/project/kerberos/src/kadmin/RCS/kpasswd.c,v 4.3 89/09/26 09:33:02 jtkohl Exp ";
#endif
static const char rcsid[] =
	"$Id$";
#endif	lint

/*
 * kpasswd
 * change your password with kerberos
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <pwd.h>
#include "kadm.h"

#include "extern.h"

extern void krb_set_tkt_string();
static void go_home(char *, int);


int krb_passwd(char *uname, char *iflag, char *rflag, char *uflag)
{
    char name[ANAME_SZ];	/* name of user */
    char inst[INST_SZ];		/* instance of user */
    char realm[REALM_SZ];	/* realm of user */
    char default_name[ANAME_SZ];
    char default_inst[INST_SZ];
    char default_realm[REALM_SZ];
    int realm_given = 0;	/* True if realm was give on cmdline */
    int use_default = 1;	/* True if we should use default name */
    struct passwd *pw;
    int status;			/* return code */
    des_cblock new_key;
    int c;
    extern char *optarg;
    extern int optind;
    char tktstring[MAXPATHLEN];

    void get_pw_new_key();

#ifdef NOENCRYPTION
#define read_long_pw_string placebo_read_pw_string
#else
#define read_long_pw_string des_read_pw_string
#endif
    int read_long_pw_string();

    bzero(name, sizeof(name));
    bzero(inst, sizeof(inst));
    bzero(realm, sizeof(realm));

    if (krb_get_tf_fullname(TKT_FILE, default_name, default_inst,
			    default_realm) != KSUCCESS) {
	pw = getpwuid((int) getuid());
	if (pw) {
		strcpy(default_name, pw->pw_name);
	} else {
	    /* seems like a null name is kinda silly */
		strcpy(default_name, "");
	}
	strcpy(default_inst, "");
	if (krb_get_lrealm(default_realm, 1) != KSUCCESS)
	    strcpy(default_realm, KRB_REALM);
    }

    if(uflag) {
	    if (status = kname_parse(name, inst, realm, uflag)) {
		    errx(2, "Kerberos error: %s", krb_err_txt[status]);
	    }
	    if (realm[0])
		realm_given++;
	    else
		if (krb_get_lrealm(realm, 1) != KSUCCESS)
		    strcpy(realm, KRB_REALM);
    }

    if(uname) {
	    if (k_isname(uname)) {
		    strncpy(name, uname, sizeof(name) - 1);
	    } else {
		    errx(1, "bad name: %s", uname);
	    }
    }

    if(iflag) {
	    if (k_isinst(iflag)) {
		    strncpy(inst, iflag, sizeof(inst) - 1);
	    } else {
		    errx(1, "bad instance: %s", iflag);
	    }
    }

    if(rflag) {
	    if (k_isrealm(rflag)) {
		    strncpy(realm, rflag, sizeof(realm) - 1);
		    realm_given++;
	    } else {
		    errx(1, "bad realm: %s", rflag);
	    }
    }

    if(uname || iflag || rflag || uflag) use_default = 0;

    if (use_default) {
	strcpy(name, default_name);
	strcpy(inst, default_inst);
	strcpy(realm, default_realm);
    } else {
	if (!name[0])
	    strcpy(name, default_name);
	if (!realm[0])
	    strcpy(realm, default_realm);
    }

    (void) sprintf(tktstring, "/tmp/tkt_cpw_%d",getpid());
    krb_set_tkt_string(tktstring);

    get_pw_new_key(new_key, name, inst, realm, realm_given);

    if ((status = kadm_init_link("changepw", KRB_MASTER, realm))
	!= KADM_SUCCESS)
	com_err("kpasswd", status, "while initializing");
    else if ((status = kadm_change_pw(new_key)) != KADM_SUCCESS)
	com_err("kpasswd", status, " attempting to change password.");

    if (status != KADM_SUCCESS)
	fprintf(stderr,"Password NOT changed.\n");
    else
	printf("Password changed.\n");

    (void) dest_tkt();
    if (status)
	exit(2);
    else
	exit(0);
}

void get_pw_new_key(new_key, name, inst, realm, print_realm)
  des_cblock new_key;
  char *name;
  char *inst;
  char *realm;
  int print_realm;		/* True if realm was give on cmdline */
{
    char ppromp[40+ANAME_SZ+INST_SZ+REALM_SZ]; /* for the password prompt */
    char pword[MAX_KPW_LEN];	               /* storage for the password */
    char npromp[40+ANAME_SZ+INST_SZ+REALM_SZ]; /* for the password prompt */

    char local_realm[REALM_SZ];
    int status;

    /*
     * We don't care about failure; this is to determine whether or
     * not to print the realm in the prompt for a new password.
     */
    (void) krb_get_lrealm(local_realm, 1);

    if (strcmp(local_realm, realm))
	print_realm++;

    (void) sprintf(ppromp,"Old password for %s%s%s%s%s:",
		   name, *inst ? "." : "", inst,
		   print_realm ? "@" : "", print_realm ? realm : "");
    if (read_long_pw_string(pword, sizeof(pword)-1, ppromp, 0)) {
	fprintf(stderr, "Error reading old password.\n");
	exit(1);
    }

    if ((status = krb_get_pw_in_tkt(name, inst, realm, PWSERV_NAME,
				    KADM_SINST, 1, pword)) != KSUCCESS) {
	if (status == INTK_BADPW) {
	    printf("Incorrect old password.\n");
	    exit(0);
	}
	else {
	    fprintf(stderr, "Kerberos error: %s\n", krb_err_txt[status]);
	    exit(1);
	}
    }
    bzero(pword, sizeof(pword));
    do {
	(void) sprintf(npromp,"New Password for %s%s%s%s%s:",
		       name, *inst ? "." : "", inst,
		       print_realm ? "@" : "", print_realm ? realm : "");
	if (read_long_pw_string(pword, sizeof(pword)-1, npromp, 1))
	    go_home("Error reading new password, password unchanged.\n",0);
	if (strlen(pword) == 0)
	    printf("Null passwords are not allowed; try again.\n");
    } while (strlen(pword) == 0);

#ifdef NOENCRYPTION
    bzero((char *) new_key, sizeof(des_cblock));
    new_key[0] = (unsigned char) 1;
#else
    (void) des_string_to_key(pword, (des_cblock *)new_key);
#endif
    bzero(pword, sizeof(pword));
}

static void
go_home(str,x)
  char *str;
  int x;
{
    fprintf(stderr, str, x);
    (void) dest_tkt();
    exit(1);
}
