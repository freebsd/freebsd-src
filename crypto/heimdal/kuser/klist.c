/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
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

#include "kuser_locl.h"
#include "rtbl.h"

RCSID("$Id: klist.c,v 1.62 2001/01/25 12:37:01 assar Exp $");

static char*
printable_time(time_t t)
{
    static char s[128];
    strcpy(s, ctime(&t)+ 4);
    s[15] = 0;
    return s;
}

static char*
printable_time_long(time_t t)
{
    static char s[128];
    strcpy(s, ctime(&t)+ 4);
    s[20] = 0;
    return s;
}

#define COL_ISSUED		"  Issued"
#define COL_EXPIRES		"  Expires"
#define COL_FLAGS		"Flags"
#define COL_PRINCIPAL		"  Principal"
#define COL_PRINCIPAL_KVNO	"  Principal (kvno)"

static void
print_cred(krb5_context context, krb5_creds *cred, rtbl_t ct, int do_flags)
{
    char *str;
    krb5_error_code ret;
    krb5_timestamp sec;

    krb5_timeofday (context, &sec);


    if(cred->times.starttime)
	rtbl_add_column_entry(ct, COL_ISSUED,
			      printable_time(cred->times.starttime));
    else
	rtbl_add_column_entry(ct, COL_ISSUED,
			      printable_time(cred->times.authtime));
    
    if(cred->times.endtime > sec)
	rtbl_add_column_entry(ct, COL_EXPIRES,
			      printable_time(cred->times.endtime));
    else
	rtbl_add_column_entry(ct, COL_EXPIRES, ">>>Expired<<<");
    ret = krb5_unparse_name (context, cred->server, &str);
    if (ret)
	krb5_err(context, 1, ret, "krb5_unparse_name");
    rtbl_add_column_entry(ct, COL_PRINCIPAL, str);
    if(do_flags) {
	char s[16], *sp = s;
	if(cred->flags.b.forwardable)
	    *sp++ = 'F';
	if(cred->flags.b.forwarded)
	    *sp++ = 'f';
	if(cred->flags.b.proxiable)
	    *sp++ = 'P';
	if(cred->flags.b.proxy)
	    *sp++ = 'p';
	if(cred->flags.b.may_postdate)
	    *sp++ = 'D';
	if(cred->flags.b.postdated)
	    *sp++ = 'd';
	if(cred->flags.b.renewable)
	    *sp++ = 'R';
	if(cred->flags.b.initial)
	    *sp++ = 'I';
	if(cred->flags.b.invalid)
	    *sp++ = 'i';
	if(cred->flags.b.pre_authent)
	    *sp++ = 'A';
	if(cred->flags.b.hw_authent)
	    *sp++ = 'H';
	*sp++ = '\0';
	rtbl_add_column_entry(ct, COL_FLAGS, s);
    }
    free(str);
}

static void
print_cred_verbose(krb5_context context, krb5_creds *cred)
{
    int j;
    char *str;
    krb5_error_code ret;
    int first_flag;
    krb5_timestamp sec;

    krb5_timeofday (context, &sec);

    ret = krb5_unparse_name(context, cred->server, &str);
    if(ret)
	exit(1);
    printf("Server: %s\n", str);
    free (str);
    {
	Ticket t;
	size_t len;
	char *s;

	decode_Ticket(cred->ticket.data, cred->ticket.length, &t, &len);
	ret = krb5_enctype_to_string(context, t.enc_part.etype, &s);
	printf("Ticket etype: ");
	if (ret == 0) {
	    printf("%s", s);
	    free(s);
	} else {
	    printf("unknown(%d)", t.enc_part.etype);
	}
	if(t.enc_part.kvno)
	    printf(", kvno %d", *t.enc_part.kvno);
	printf("\n");
	if(cred->session.keytype != t.enc_part.etype) {
	    ret = krb5_keytype_to_string(context, cred->session.keytype, &str);
	    if(ret == KRB5_PROG_KEYTYPE_NOSUPP)
		ret = krb5_enctype_to_string(context, cred->session.keytype, 
					     &str);
	    if(ret)
		krb5_warn(context, ret, "session keytype");
	    else {
		printf("Session key: %s\n", str);
		free(str);
	    }
	}
	free_Ticket(&t);
    }
    printf("Auth time:  %s\n", printable_time_long(cred->times.authtime));
    if(cred->times.authtime != cred->times.starttime)
	printf("Start time: %s\n", printable_time_long(cred->times.starttime));
    printf("End time:   %s", printable_time_long(cred->times.endtime));
    if(sec > cred->times.endtime)
	printf(" (expired)");
    printf("\n");
    if(cred->flags.b.renewable)
	printf("Renew till: %s\n", 
	       printable_time_long(cred->times.renew_till));
    printf("Ticket flags: ");
#define PRINT_FLAG2(f, s) if(cred->flags.b.f) { if(!first_flag) printf(", "); printf("%s", #s); first_flag = 0; }
#define PRINT_FLAG(f) PRINT_FLAG2(f, f)
    first_flag = 1;
    PRINT_FLAG(forwardable);
    PRINT_FLAG(forwarded);
    PRINT_FLAG(proxiable);
    PRINT_FLAG(proxy);
    PRINT_FLAG2(may_postdate, may-postdate);
    PRINT_FLAG(postdated);
    PRINT_FLAG(invalid);
    PRINT_FLAG(renewable);
    PRINT_FLAG(initial);
    PRINT_FLAG2(pre_authent, pre-authenticated);
    PRINT_FLAG2(hw_authent, hw-authenticated);
    PRINT_FLAG2(transited_policy_checked, transited-policy-checked);
    PRINT_FLAG2(ok_as_delegate, ok-as-delegate);
    PRINT_FLAG(anonymous);
    printf("\n");
    printf("Addresses: ");
    for(j = 0; j < cred->addresses.len; j++){
	char buf[128];
	size_t len;
	if(j) printf(", ");
	ret = krb5_print_address(&cred->addresses.val[j], 
				 buf, sizeof(buf), &len);

	if(ret == 0)
	    printf("%s", buf);
    }
    printf("\n\n");
}

/*
 * Print all tickets in `ccache' on stdout, verbosily iff do_verbose.
 */

static void
print_tickets (krb5_context context,
	       krb5_ccache ccache,
	       krb5_principal principal,
	       int do_verbose,
	       int do_flags)
{
    krb5_error_code ret;
    char *str;
    krb5_cc_cursor cursor;
    krb5_creds creds;

    rtbl_t ct = NULL;

    ret = krb5_unparse_name (context, principal, &str);
    if (ret)
	krb5_err (context, 1, ret, "krb5_unparse_name");

    printf ("%17s: %s:%s\n", 
	    "Credentials cache",
	    krb5_cc_get_type(context, ccache),
	    krb5_cc_get_name(context, ccache));
    printf ("%17s: %s\n", "Principal", str);
    free (str);
    
    if(do_verbose)
	printf ("%17s: %d\n", "Cache version",
		krb5_cc_get_version(context, ccache));
    
    if (do_verbose && context->kdc_sec_offset) {
	char buf[BUFSIZ];
	int val;
	int sig;

	val = context->kdc_sec_offset;
	sig = 1;
	if (val < 0) {
	    sig = -1;
	    val = -val;
	}
	
	unparse_time (val, buf, sizeof(buf));

	printf ("%17s: %s%s\n", "KDC time offset",
		sig == -1 ? "-" : "", buf);
    }

    printf("\n");

    ret = krb5_cc_start_seq_get (context, ccache, &cursor);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_start_seq_get");

    if(!do_verbose) {
	ct = rtbl_create();
	rtbl_add_column(ct, COL_ISSUED, 0);
	rtbl_add_column(ct, COL_EXPIRES, 0);
	if(do_flags)
	    rtbl_add_column(ct, COL_FLAGS, 0);
	rtbl_add_column(ct, COL_PRINCIPAL, 0);
	rtbl_set_prefix(ct, "  ");
	rtbl_set_column_prefix(ct, COL_ISSUED, "");
    }
    while (krb5_cc_next_cred (context,
			      ccache,
			      &creds,
			      &cursor) == 0) {
	if(do_verbose){
	    print_cred_verbose(context, &creds);
	}else{
	    print_cred(context, &creds, ct, do_flags);
	}
	krb5_free_creds_contents (context, &creds);
    }
    ret = krb5_cc_end_seq_get (context, ccache, &cursor);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_end_seq_get");
    if(!do_verbose) {
	rtbl_format(ct, stdout);
	rtbl_destroy(ct);
    }
}

/*
 * Check if there's a tgt for the realm of `principal' and ccache and
 * if so return 0, else 1
 */

static int
check_for_tgt (krb5_context context,
	       krb5_ccache ccache,
	       krb5_principal principal)
{
    krb5_error_code ret;
    krb5_creds pattern;
    krb5_creds creds;
    krb5_realm *client_realm;
    int expired;

    client_realm = krb5_princ_realm (context, principal);

    ret = krb5_make_principal (context, &pattern.server,
			       *client_realm, KRB5_TGS_NAME, *client_realm,
			       NULL);
    if (ret)
	krb5_err (context, 1, ret, "krb5_make_principal");

    ret = krb5_cc_retrieve_cred (context, ccache, 0, &pattern, &creds);
    expired = time(NULL) > creds.times.endtime;
    krb5_free_principal (context, pattern.server);
    krb5_free_creds_contents (context, &creds);
    if (ret) {
	if (ret == KRB5_CC_END)
	    return 1;
	krb5_err (context, 1, ret, "krb5_cc_retrieve_cred");
    }
    return expired;
}

#ifdef KRB4
/* prints the approximate kdc time differential as something human
   readable */

static void
print_time_diff(int do_verbose)
{
    int d = abs(krb_get_kdc_time_diff());
    char buf[80];

    if ((do_verbose && d > 0) || d > 60) {
	unparse_time_approx (d, buf, sizeof(buf));
	printf ("Time diff:\t%s\n", buf);
    }
}

/*
 * return a short representation of `dp' in string form.
 */

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

/*
 * Print a list of all the v4 tickets
 */

static int
display_v4_tickets (int do_verbose)
{
    char *file;
    int ret;
    krb_principal princ;
    CREDENTIALS cred;
    int found = 0;

    rtbl_t ct;

    file = getenv ("KRBTKFILE");
    if (file == NULL)
	file = TKT_FILE;

    printf("v4-ticket file:	%s\n", file);

    ret = krb_get_tf_realm (file, princ.realm);
    if (ret) {
	warnx ("%s", krb_get_err_text(ret));
	return 1;
    }

    ret = tf_init (file, R_TKT_FIL);
    if (ret) {
	warnx ("tf_init: %s", krb_get_err_text(ret));
	return 1;
    }
    ret = tf_get_pname (princ.name);
    if (ret) {
	tf_close ();
	warnx ("tf_get_pname: %s", krb_get_err_text(ret));
	return 1;
    }
    ret = tf_get_pinst (princ.instance);
    if (ret) {
	tf_close ();
	warnx ("tf_get_pname: %s", krb_get_err_text(ret));
	return 1;
    }

    printf("Principal:\t%s\n", krb_unparse_name (&princ));
    print_time_diff(do_verbose);
    printf("\n");

    ct = rtbl_create();
    rtbl_add_column(ct, COL_ISSUED, 0);
    rtbl_add_column(ct, COL_EXPIRES, 0);
    if (do_verbose)
	rtbl_add_column(ct, COL_PRINCIPAL_KVNO, 0);
    else
	rtbl_add_column(ct, COL_PRINCIPAL, 0);
    rtbl_set_prefix(ct, "  ");
    rtbl_set_column_prefix(ct, COL_ISSUED, "");

    while ((ret = tf_get_cred(&cred)) == KSUCCESS) {
	struct timeval tv;
	char buf1[20], buf2[20];
	const char *pp;

	found++;

	strlcpy(buf1,
		short_date(cred.issue_date),
		sizeof(buf1));
	cred.issue_date = krb_life_to_time(cred.issue_date, cred.lifetime);
	krb_kdctimeofday(&tv);
	if (do_verbose || tv.tv_sec < (unsigned long) cred.issue_date)
	    strlcpy(buf2,
		    short_date(cred.issue_date),
		    sizeof(buf2));
	else
	    strlcpy(buf2,
		    ">>> Expired <<<",
		    sizeof(buf2));
	rtbl_add_column_entry(ct, COL_ISSUED, buf1);
	rtbl_add_column_entry(ct, COL_EXPIRES, buf2);
	pp = krb_unparse_name_long(cred.service,
				   cred.instance,
				   cred.realm);
	if (do_verbose) {
	    char *tmp;

	    asprintf(&tmp, "%s (%d)", pp, cred.kvno);
	    rtbl_add_column_entry(ct, COL_PRINCIPAL_KVNO, tmp);
	    free(tmp);
	} else {
	    rtbl_add_column_entry(ct, COL_PRINCIPAL, pp);
	}
    }
    rtbl_format(ct, stdout);
    rtbl_destroy(ct);
    if (!found && ret == EOF)
	printf("No tickets in file.\n");
    tf_close();
    
    /*
     * should do NAT stuff here
     */
    return 0;
}

/*
 * Print a list of all AFS tokens
 */

static void
display_tokens(int do_verbose)
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
        unsigned char *cell;
	struct ClearToken ct;
	unsigned char *r = t;
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
	cell = r;

	gettimeofday (&tv, NULL);
	strlcpy (buf1, printable_time(ct.BeginTimestamp),
			 sizeof(buf1));
	if (do_verbose || tv.tv_sec < ct.EndTimestamp)
	    strlcpy (buf2, printable_time(ct.EndTimestamp),
			     sizeof(buf2));
	else
	    strlcpy (buf2, ">>> Expired <<<", sizeof(buf2));

	printf("%s  %s  ", buf1, buf2);

	if ((ct.EndTimestamp - ct.BeginTimestamp) & 1)
	  printf("User's (AFS ID %d) tokens for %s", ct.ViceId, cell);
	else
	  printf("Tokens for %s", cell);
	if (do_verbose)
	    printf(" (%d)", ct.AuthHandle);
	putchar('\n');
    }
}
#endif /* KRB4 */

/*
 * display the ccache in `cred_cache'
 */

static int
display_v5_ccache (const char *cred_cache, int do_test, int do_verbose, 
		   int do_flags)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache ccache;
    krb5_principal principal;
    int exit_status = 0;

    ret = krb5_init_context (&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    if(cred_cache) {
	ret = krb5_cc_resolve(context, cred_cache, &ccache);
	if (ret)
	    krb5_err (context, 1, ret, "%s", cred_cache);
    } else {
	ret = krb5_cc_default (context, &ccache);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_cc_resolve");
    }

    ret = krb5_cc_get_principal (context, ccache, &principal);
    if (ret) {
	if(ret == ENOENT) {
	    if (!do_test)
		krb5_warnx(context, "No ticket file: %s",
			   krb5_cc_get_name(context, ccache));
	    return 1;
	} else
	    krb5_err (context, 1, ret, "krb5_cc_get_principal");
    }
    if (do_test)
	exit_status = check_for_tgt (context, ccache, principal);
    else
	print_tickets (context, ccache, principal, do_verbose, do_flags);

    ret = krb5_cc_close (context, ccache);
    if (ret)
	krb5_err (context, 1, ret, "krb5_cc_close");

    krb5_free_principal (context, principal);
    krb5_free_context (context);
    return exit_status;
}

static int version_flag = 0;
static int help_flag	= 0;
static int do_verbose	= 0;
static int do_test	= 0;
#ifdef KRB4
static int do_v4	= 1;
static int do_tokens	= 0;
#endif
static int do_v5	= 1;
static char *cred_cache;
static int do_flags = 0;

static struct getargs args[] = {
    { NULL, 'f', arg_flag, &do_flags },
    { "cache",			'c', arg_string, &cred_cache,
      "credentials cache to list", "cache" },
    { "test",			't', arg_flag, &do_test,
      "test for having tickets", NULL },
    { NULL,			's', arg_flag, &do_test },
#ifdef KRB4
    { "v4",			'4',	arg_flag, &do_v4,
      "display v4 tickets", NULL },
    { "tokens",			'T',   arg_flag, &do_tokens,
      "display AFS tokens", NULL },
#endif
    { "v5",			'5',	arg_flag, &do_v5,
      "display v5 cred cache", NULL},
    { "verbose",		'v', arg_flag, &do_verbose,
      "verbose output", NULL },
    { NULL,			'a', arg_flag, &do_verbose },
    { NULL,			'n', arg_flag, &do_verbose },
    { "version", 		0,   arg_flag, &version_flag, 
      "print version", NULL },
    { "help",			0,   arg_flag, &help_flag, 
      NULL, NULL}
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main (int argc, char **argv)
{
    int optind = 0;
    int exit_status = 0;

    set_progname (argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optind;
    argv += optind;

    if (argc != 0)
	usage (1);

    if (do_v5)
	exit_status = display_v5_ccache (cred_cache, do_test, 
					 do_verbose, do_flags);

#ifdef KRB4
    if (!do_test) {
	if (do_v4) {
	    if (do_v5)
		printf ("\n");
	    display_v4_tickets (do_verbose);
	}
	if (do_tokens && k_hasafs ()) {
	    if (do_v4 || do_v5)
		printf ("\n");
	    display_tokens (do_verbose);
	}
    }
#endif

    return exit_status;
}
