/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * Lists your current Kerberos tickets.
 * Written by Bill Sommerfeld, MIT Project Athena.
 */

#include "kuser_locl.h"

#if defined(HAVE_SYS_IOCTL_H) && SunOS != 4
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif

#include <kafs.h>

RCSID("$Id: klist.c,v 1.28 1997/05/26 17:33:50 bg Exp $");

static int option_verbose = 0;

static char *
short_date(int32_t dp)
{
    char *cp;
    time_t t = (time_t)dp;

    if (t == (time_t)(-1L)) return "***  Never  *** ";
    cp = ctime(&t) + 4;
    cp[15] = '\0';
    return (cp);
}

static void
display_tktfile(char *file, int tgt_test, int long_form)
{
    krb_principal pr;
    char    buf1[20], buf2[20];
    int     k_errno;
    CREDENTIALS c;
    int     header = 1;

    if ((file == NULL) && ((file = getenv("KRBTKFILE")) == NULL))
	file = TKT_FILE;

    if (long_form)
	printf("Ticket file:	%s\n", file);

    /* 
     * Since krb_get_tf_realm will return a ticket_file error, 
     * we will call tf_init and tf_close first to filter out
     * things like no ticket file.  Otherwise, the error that 
     * the user would see would be 
     * klist: can't find realm of ticket file: No ticket file (tf_util)
     * instead of
     * klist: No ticket file (tf_util)
     */

    /* Open ticket file */
    if ((k_errno = tf_init(file, R_TKT_FIL))) {
	if (!tgt_test)
	    warnx("%s", krb_get_err_text(k_errno));
	exit(1);
    }
    /* Close ticket file */
    tf_close();

    /* 
     * We must find the realm of the ticket file here before calling
     * tf_init because since the realm of the ticket file is not
     * really stored in the principal section of the file, the
     * routine we use must itself call tf_init and tf_close.
     */
    if ((k_errno = krb_get_tf_realm(file, pr.realm)) != KSUCCESS) {
	if (!tgt_test)
	    warnx("can't find realm of ticket file: %s", 
		  krb_get_err_text(k_errno));
	exit(1);
    }

    /* Open ticket file */
    if ((k_errno = tf_init(file, R_TKT_FIL))) {
	if (!tgt_test)
	    warnx("%s", krb_get_err_text(k_errno));
	exit(1);
    }
    /* Get principal name and instance */
    if ((k_errno = tf_get_pname(pr.name)) ||
	(k_errno = tf_get_pinst(pr.instance))) {
	if (!tgt_test)
	    warnx("%s", krb_get_err_text(k_errno));
	exit(1);
    }

    /* 
     * You may think that this is the obvious place to get the
     * realm of the ticket file, but it can't be done here as the
     * routine to do this must open the ticket file.  This is why 
     * it was done before tf_init.
     */
       
    if (!tgt_test && long_form)
	printf("Principal:\t%s\n\n", krb_unparse_name(&pr));
    while ((k_errno = tf_get_cred(&c)) == KSUCCESS) {
	if (!tgt_test && long_form && header) {
	    printf("%-15s  %-15s  %s%s\n",
		   "  Issued", "  Expires", "  Principal", 
		   option_verbose ? " (kvno)" : "");
	    header = 0;
	}
	if (tgt_test) {
	    c.issue_date = krb_life_to_time(c.issue_date, c.lifetime);
	    if (!strcmp(c.service, KRB_TICKET_GRANTING_TICKET) &&
		!strcmp(c.instance, pr.realm)) {
		if (time(0) < c.issue_date)
		    exit(0);		/* tgt hasn't expired */
		else
		    exit(1);		/* has expired */
	    }
	    continue;			/* not a tgt */
	}
	if (long_form) {
	    strcpy(buf1, short_date(c.issue_date));
	    c.issue_date = krb_life_to_time(c.issue_date, c.lifetime);
	    if (time(0) < (unsigned long) c.issue_date)
	        strcpy(buf2, short_date(c.issue_date));
	    else
	        strcpy(buf2, ">>> Expired <<<");
	    printf("%s  %s  ", buf1, buf2);
	}
	printf("%s", krb_unparse_name_long(c.service, c.instance, c.realm));
	if(long_form && option_verbose)
	  printf(" (%d)", c.kvno);
	printf("\n");
    }
    if (tgt_test)
	exit(1);			/* no tgt found */
    if (header && long_form && k_errno == EOF) {
	printf("No tickets in file.\n");
    }
}

/* adapted from getst() in librkb */
/*
 * ok_getst() takes a file descriptor, a string and a count.  It reads
 * from the file until either it has read "count" characters, or until
 * it reads a null byte.  When finished, what has been read exists in
 * the given string "s".  If "count" characters were actually read, the
 * last is changed to a null, so the returned string is always null-
 * terminated.  ok_getst() returns the number of characters read, including
 * the null terminator.
 *
 * If there is a read error, it returns -1 (like the read(2) system call)
 */

static int
ok_getst(int fd, char *s, int n)
{
    int count = n;
    int err;
    while ((err = read(fd, s, 1)) > 0 && --count)
        if (*s++ == '\0')
            return (n - count);
    if (err < 0)
	return(-1);
    *s = '\0';
    return (n - count);
}

static void
display_tokens()
{
    u_int32_t i;
    unsigned char t[128];
    struct ViceIoctl parms;
    struct ClearToken ct;
    int size_secret_tok, size_public_tok;

    parms.in = (void *)&i;
    parms.in_size = sizeof(i);
    parms.out = (void *)t;
    parms.out_size = sizeof(t);

    for (i = 0; k_pioctl(NULL, VIOCGETTOK, &parms, 0) == 0; i++) {
        char *cell;
	memcpy(&size_secret_tok, t, 4);
	memcpy(&size_public_tok, t + 4 + size_secret_tok, 4);
	memcpy(&ct, t + 4 + size_secret_tok + 4, size_public_tok);
	cell = t + 4 + size_secret_tok + 4 + size_public_tok + 4;

	printf("%-15s  ", short_date(ct.BeginTimestamp));
	printf("%-15s  ", short_date(ct.EndTimestamp));
	if ((ct.EndTimestamp - ct.BeginTimestamp) & 1)
	  printf("User's (AFS ID %d) tokens for %s", ct.ViceId, cell);
	else
	  printf("Tokens for %s", cell);
	if (option_verbose)
	    printf(" (%d)", ct.AuthHandle);
	putchar('\n');
    }
}

static void
display_srvtab(char *file)
{
    int stab;
    char serv[SNAME_SZ];
    char inst[INST_SZ];
    char rlm[REALM_SZ];
    unsigned char key[8];
    unsigned char vno;
    int count;

    printf("Server key file:   %s\n", file);
	
    if ((stab = open(file, O_RDONLY, 0400)) < 0) {
	perror(file);
	exit(1);
    }
    printf("%-15s %-15s %-10s %s\n","Service","Instance","Realm",
	   "Key Version");
    printf("------------------------------------------------------\n");

    /* argh. getst doesn't return error codes, it silently fails */
    while (((count = ok_getst(stab, serv, SNAME_SZ)) > 0)
	   && ((count = ok_getst(stab, inst, INST_SZ)) > 0)
	   && ((count = ok_getst(stab, rlm, REALM_SZ)) > 0)) {
	if (((count = read(stab,  &vno,1)) != 1) ||
	     ((count = read(stab, key,8)) != 8)) {
	    if (count < 0)
		err(1, "reading from key file");
	    else
		errx(1, "key file truncated");
	}
	printf("%-15s %-15s %-15s %d\n",serv,inst,rlm,vno);
    }
    if (count < 0)
	warn(file);
    close(stab);
}

static void
usage(void)
{
    fprintf(stderr,
	    "Usage: %s [ -v | -s | -t ] [ -f filename ] [-tokens] [-srvtab ]\n",
	    __progname);
    exit(1);
}

/* ARGSUSED */
int
main(int argc, char **argv)
{
    int     long_form = 1;
    int     tgt_test = 0;
    int     do_srvtab = 0;
    int     do_tokens = 0;
    char   *tkt_file = NULL;

    set_progname(argv[0]);

    while (*(++argv)) {
	if (!strcmp(*argv, "-v")) {
	    option_verbose = 1;
	    continue;
	}
	if (!strcmp(*argv, "-s")) {
	    long_form = 0;
	    continue;
	}
	if (!strcmp(*argv, "-t")) {
	    tgt_test = 1;
	    long_form = 0;
	    continue;
	}
	if (strcmp(*argv, "-tokens") == 0
	    || strcmp(*argv, "-T") == 0) {
	    do_tokens = k_hasafs();
	    continue;
	}
	if (!strcmp(*argv, "-l")) {	/* now default */
	    continue;
	}
	if (!strncmp(*argv, "-f", 2)) {
	    if (*(++argv)) {
		tkt_file = *argv;
		continue;
	    } else
		usage();
	}
	if (!strcmp(*argv, "-srvtab")) {
		if (tkt_file == NULL)	/* if no other file spec'ed,
					   set file to default srvtab */
		    tkt_file = KEYFILE;
		do_srvtab = 1;
		continue;
	}
	usage();
    }

    if (do_srvtab)
	display_srvtab(tkt_file);
    else
	display_tktfile(tkt_file, tgt_test, long_form);
    if (long_form && do_tokens)
	display_tokens();
    exit(0);
}
