/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * Routine to initialize user to Kerberos.  Prompts optionally for
 * user, instance and realm.  Authenticates user and gets a ticket
 * for the Kerberos ticket-granting service for future use. 
 *
 * Options are: 
 *
 *   -i[instance]
 *   -r[realm]
 *   -v[erbose]
 *   -l[ifetime]
 *   -p
 */

#include "kuser_locl.h"

RCSID("$Id: kinit.c,v 1.15 1997/03/30 18:58:46 assar Exp $");

#define	LIFE	DEFAULT_TKT_LIFE /* lifetime of ticket in 5-minute units */
#define CHPASSLIFE 2

static void
get_input(char *s, int size, FILE *stream)
{
    char *p;

    if (fgets(s, size, stream) == NULL)
	exit(1);
    if ( (p = strchr(s, '\n')) != NULL)
	*p = '\0';
}


static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-irvlp] [name]\n", __progname);
    exit(1);
}

int
main(int argc, char **argv)
{
    char    aname[ANAME_SZ];
    char    inst[INST_SZ];
    char    realm[REALM_SZ];
    char    buf[MaxHostNameLen];
    char    name[MAX_K_NAME_SZ];
    char   *username = NULL;
    int     iflag, rflag, vflag, lflag, pflag, lifetime, k_errno;
    int	    i;

    set_progname (argv[0]);

    *inst = *realm = '\0';
    iflag = rflag = vflag = lflag = pflag = 0;
    lifetime = LIFE;
    set_progname(argv[0]);

    while (--argc) {
	if ((*++argv)[0] != '-') {
	    if (username)
		usage();
	    username = *argv;
	    continue;
	}
	for (i = 1; (*argv)[i] != '\0'; i++)
	    switch ((*argv)[i]) {
	    case 'i':		/* Instance */
		++iflag;
		continue;
	    case 'r':		/* Realm */
		++rflag;
		continue;
	    case 'v':		/* Verbose */
		++vflag;
		continue;
	    case 'l':
		++lflag;
		continue;
	    case 'p':
		++pflag;	/* chpass-tickets */
		lifetime = CHPASSLIFE;
		break;
	    default:
		usage();
	    }
    }
    if (username &&
	(k_errno = kname_parse(aname, inst, realm, username)) != KSUCCESS) {
	warnx("%s", krb_get_err_text(k_errno));
	iflag = rflag = 1;
	username = NULL;
    }
    if (k_gethostname(buf, MaxHostNameLen)) 
	err(1, "k_gethostname failed");
    printf("%s (%s)\n", ORGANIZATION, buf);
    if (username) {
	printf("Kerberos Initialization for \"%s", aname);
	if (*inst)
	    printf(".%s", inst);
	if (*realm)
	    printf("@%s", realm);
	printf("\"\n");
    } else {
	printf("Kerberos Initialization\n");
	printf("Kerberos name: ");
	get_input(name, sizeof(name), stdin);
	if (!*name)
	    return 0;
	if ((k_errno = kname_parse(aname, inst, realm, name)) != KSUCCESS )
	    errx(1, "%s", krb_get_err_text(k_errno));
    }
    /* optional instance */
    if (iflag) {
	printf("Kerberos instance: ");
	get_input(inst, sizeof(inst), stdin);
	if (!k_isinst(inst))
	    errx(1, "bad Kerberos instance format");
    }
    if (rflag) {
	printf("Kerberos realm: ");
	get_input(realm, sizeof(realm), stdin);
	if (!k_isrealm(realm))
	    errx(1, "bad Kerberos realm format");
    }
    if (lflag) {
	 printf("Kerberos ticket lifetime (minutes): ");
	 get_input(buf, sizeof(buf), stdin);
	 lifetime = atoi(buf);
	 if (lifetime < 5)
	      lifetime = 1;
	 else
	      lifetime = krb_time_to_life(0, lifetime*60);
	 /* This should be changed if the maximum ticket lifetime */
	 /* changes */
	 if (lifetime > 255)
	      lifetime = 255;
    }
    if (!*realm && krb_get_lrealm(realm, 1))
	errx(1, "krb_get_lrealm failed");
    k_errno = krb_get_pw_in_tkt(aname, inst, realm,
				pflag ? PWSERV_NAME : 
				KRB_TICKET_GRANTING_TICKET,
				pflag ? KADM_SINST  : realm,
				lifetime, 0);
    if (vflag) {
	printf("Kerberos realm %s:\n", realm);
	printf("%s\n", krb_get_err_text(k_errno));
    } else if (k_errno)
	errx(1, "%s", krb_get_err_text(k_errno));
    exit(0);
}
