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

#if defined(HAVE_SYS_IOCTL_H) && SunOS != 40
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif

#include <kafs.h>

#include <parse_time.h>

RCSID("$Id: klist.c,v 1.44.2.3 2000/10/18 20:38:29 assar Exp $");

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

/* prints the approximate kdc time differential as something human
   readable */

static void
print_time_diff(void)
{
    int d = abs(krb_get_kdc_time_diff());
    char buf[80];

    if ((option_verbose && d > 0) || d > 60) {
	unparse_time_approx (d, buf, sizeof(buf));
	printf ("Time diff:\t%s\n", buf);
    }
}

static
int
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
	return 1;
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
	return 1;
    }

    /* Open ticket file */
    if ((k_errno = tf_init(file, R_TKT_FIL))) {
	if (!tgt_test)
	    warnx("%s", krb_get_err_text(k_errno));
	return 1;
    }
    /* Get principal name and instance */
    if ((k_errno = tf_get_pname(pr.name)) ||
	(k_errno = tf_get_pinst(pr.instance))) {
	if (!tgt_test)
	    warnx("%s", krb_get_err_text(k_errno));
	return 1;
    }

    /* 
     * You may think that this is the obvious place to get the
     * realm of the ticket file, but it can't be done here as the
     * routine to do this must open the ticket file.  This is why 
     * it was done before tf_init.
     */
       
    if (!tgt_test && long_form) {
	printf("Principal:\t%s\n", krb_unparse_name(&pr));
	print_time_diff();
	printf("\n");
    }
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
		    return 0;		/* tgt hasn't expired */
		else
		    return 1;		/* has expired */
	    }
	    continue;			/* not a tgt */
	}
	if (long_form) {
	    struct timeval tv;
	    strlcpy(buf1,
		    short_date(c.issue_date),
		    sizeof(buf1));
	    c.issue_date = krb_life_to_time(c.issue_date, c.lifetime);
	    krb_kdctimeofday(&tv);
	    if (option_verbose || tv.tv_sec < (unsigned long) c.issue_date)
	        strlcpy(buf2,
			short_date(c.issue_date),
			sizeof(buf2));
	    else
	        strlcpy(buf2,
			">>> Expired <<<",
			sizeof(buf2));
	    printf("%s  %s  ", buf1, buf2);
	}
	printf("%s", krb_unparse_name_long(c.service, c.instance, c.realm));
	if(long_form && option_verbose)
	    printf(" (%d)", c.kvno);
	printf("\n");
    }
    if (tgt_test)
	return 1;			/* no tgt found */
    if (header && long_form && k_errno == EOF) {
	printf("No tickets in file.\n");
    }
    tf_close();
 
    if (long_form && krb_get_config_bool("nat_in_use")) {
	char realm[REALM_SZ];
	struct in_addr addr;
 
	printf("-----\nNAT addresses\n");
 
	/* Open ticket file (again) */
	if ((k_errno = tf_init(file, R_TKT_FIL))) {
	    if (!tgt_test)
		warnx("%s", krb_get_err_text(k_errno));
	    return 1;
	}
 
	/* Get principal name and instance */
	if ((k_errno = tf_get_pname(pr.name)) ||
	    (k_errno = tf_get_pinst(pr.instance))) {
	    if (!tgt_test)
		warnx("%s", krb_get_err_text(k_errno));
	    return 1;
	}
 
	while ((k_errno = tf_get_cred_addr(realm, sizeof(realm),
					   &addr)) == KSUCCESS) {
	    printf("%s: %s\n", realm, inet_ntoa(addr));
	}
	tf_close();
    }
 
    return 0;
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
display_tokens(void)
{
    u_int32_t i;
    unsigned char t[128];
    struct ViceIoctl parms;

    parms.in = (void *)&i;
    parms.in_size = sizeof(i);
    parms.out = (void *)t;
    parms.out_size = sizeof(t);

    for (i = 0; k_pioctl(NULL, VIOCGETTOK, &parms, 0) == 0; i++) {
        int32_t size_secret_tok, size_public_tok;
        const char *cell;
	struct ClearToken ct;
	const unsigned char *r = t;
	struct timeval tv;
	char buf1[20], buf2[20];

	memcpy(&size_secret_tok, r, sizeof(size_secret_tok));
	/* dont bother about the secret token */
	r += size_secret_tok + sizeof(size_secret_tok);
	memcpy(&size_public_tok, r, sizeof(size_public_tok));
	r += sizeof(size_public_tok);
	memcpy(&ct, r, size_public_tok);
	r += size_public_tok;
	/* there is a int32_t with length of cellname, but we dont read it */
	r += sizeof(int32_t);
	cell = (const char *)r;

	krb_kdctimeofday (&tv);
	strlcpy (buf1, short_date(ct.BeginTimestamp), sizeof(buf1));
	if (option_verbose || tv.tv_sec < ct.EndTimestamp)
	    strlcpy (buf2, short_date(ct.EndTimestamp), sizeof(buf2));
	else
	    strlcpy (buf2, ">>> Expired <<<", sizeof(buf2));

	printf("%s  %s  ", buf1, buf2);

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
	warn("%s", file);
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
    int     eval;

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
		    tkt_file = (char *)KEYFILE;
		do_srvtab = 1;
		continue;
	}
	usage();
    }

    eval = 0;
    if (do_srvtab)
	display_srvtab(tkt_file);
    else
	eval = display_tktfile(tkt_file, tgt_test, long_form);
    if (long_form && do_tokens){
	printf("\nAFS tokens:\n");
	display_tokens();
    }
    exit(eval);
}
