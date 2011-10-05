/*
 * Copyright (c) 1995-1997, 1999 Kungliga Tekniska Högskolan
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

#include "otp_locl.h"
#include <getarg.h>

RCSID("$Id$");

static int listp;
static int deletep;
static int openp;
static int renewp;
static char* alg_string;
static char *user;
static int version_flag;
static int help_flag;

struct getargs args[] = {
    { "list", 'l', arg_flag, &listp, "list OTP status" },
    { "delete", 'd', arg_flag, &deletep, "delete OTP" },
    { "open", 'o', arg_flag, &openp, "open a locked OTP" },
    { "renew", 'r', arg_flag, &renewp, "securely renew OTP" },
    { "hash", 'f', arg_string, &alg_string,
      "hash algorithm (md4, md5, or sha)", "algorithm"},
    { "user", 'u', arg_string, &user,
      "user other than current user (root only)", "user" },
    { "version", 0, arg_flag, &version_flag },
    { "help", 'h', arg_flag, &help_flag }
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int code)
{
    arg_printusage(args, num_args, NULL, "[num seed]");
    exit(code);
}

/*
 * Renew the OTP for a user.
 * The pass-phrase is not required (RFC 1938/8.0)
 */

static int
renew (int argc, char **argv, OtpAlgorithm *alg, char *user)
{
    OtpContext newctx, *ctx;
    char prompt[128];
    char pw[64];
    void *dbm;
    int ret;

    newctx.alg = alg;
    newctx.user = user;
    newctx.n = atoi (argv[0]);
    strlcpy (newctx.seed, argv[1], sizeof(newctx.seed));
    strlwr(newctx.seed);
    snprintf (prompt, sizeof(prompt),
	      "[ otp-%s %u %s ]",
	      newctx.alg->name,
	      newctx.n,
	      newctx.seed);
    if (UI_UTIL_read_pw_string (pw, sizeof(pw), prompt, 0) == 0 &&
	otp_parse (newctx.key, pw, alg) == 0) {
	ctx = &newctx;
	ret = 0;
    } else
	return 1;

    dbm = otp_db_open ();
    if (dbm == NULL) {
	warnx ("otp_db_open failed");
	return 1;
    }
    otp_put (dbm, ctx);
    otp_db_close (dbm);
    return ret;
}

/*
 * Return 0 if the user could enter the next OTP.
 * I would rather have returned !=0 but it's shell-like here around.
 */

static int
verify_user_otp(char *username)
{
    OtpContext ctx;
    char passwd[OTP_MAX_PASSPHRASE + 1];
    char prompt[128], ss[256];

    if (otp_challenge (&ctx, username, ss, sizeof(ss)) != 0) {
	warnx("no otp challenge found for %s", username);
	return 1;
    }

    snprintf (prompt, sizeof(prompt), "%s's %s Password: ", username, ss);
    if(UI_UTIL_read_pw_string(passwd, sizeof(passwd)-1, prompt, 0))
	return 1;
    return otp_verify_user (&ctx, passwd);
}

/*
 * Set the OTP for a user
 */

static int
set (int argc, char **argv, OtpAlgorithm *alg, char *user)
{
    void *db;
    OtpContext ctx;
    char pw[OTP_MAX_PASSPHRASE + 1];
    int ret;
    int i;

    ctx.alg = alg;
    ctx.user = strdup (user);
    if (ctx.user == NULL)
	err (1, "out of memory");

    ctx.n = atoi (argv[0]);
    strlcpy (ctx.seed, argv[1], sizeof(ctx.seed));
    strlwr(ctx.seed);
    do {
	if (UI_UTIL_read_pw_string (pw, sizeof(pw), "Pass-phrase: ", 1))
	    return 1;
	if (strlen (pw) < OTP_MIN_PASSPHRASE)
	    printf ("Too short pass-phrase.  Use at least %d characters\n",
		    OTP_MIN_PASSPHRASE);
    } while(strlen(pw) < OTP_MIN_PASSPHRASE);
    ctx.alg->init (ctx.key, pw, ctx.seed);
    for (i = 0; i < ctx.n; ++i)
	ctx.alg->next (ctx.key);
    db = otp_db_open ();
    if(db == NULL) {
	free (ctx.user);
	err (1, "otp_db_open failed");
    }
    ret = otp_put (db, &ctx);
    otp_db_close (db);
    free (ctx.user);
    return ret;
}

/*
 * Delete otp of user from the database
 */

static int
delete_otp (int argc, char **argv, char *user)
{
    void *db;
    OtpContext ctx;
    int ret;

    db = otp_db_open ();
    if(db == NULL)
	errx (1, "otp_db_open failed");

    ctx.user = user;
    ret = otp_delete(db, &ctx);
    otp_db_close (db);
    return ret;
}

/*
 * Tell whether the user has an otp
 */

static int
has_an_otp(char *user)
{
    void *db;
    OtpContext ctx;
    int ret;

    db = otp_db_open ();
    if(db == NULL) {
	warnx ("otp_db_open failed");
	return 0; /* if no db no otp! */
    }

    ctx.user = user;
    ret = otp_simple_get(db, &ctx);

    otp_db_close (db);
    return !ret;
}

/*
 * Get and print out the otp entry for some user
 */

static void
print_otp_entry_for_name (void *db, char *user)
{
    OtpContext ctx;

    ctx.user = user;
    if (!otp_simple_get(db, &ctx)) {
	fprintf(stdout,
		"%s\totp-%s %d %s",
		ctx.user, ctx.alg->name, ctx.n, ctx.seed);
	if (ctx.lock_time)
	    fprintf(stdout,
		    "\tlocked since %s",
		    ctime(&ctx.lock_time));
	else
	    fprintf(stdout, "\n");
    }
}

static int
open_otp (int argc, char **argv, char *user)
{
    void *db;
    OtpContext ctx;
    int ret;

    db = otp_db_open ();
    if (db == NULL)
	errx (1, "otp_db_open failed");

    ctx.user = user;
    ret = otp_simple_get (db, &ctx);
    if (ret == 0)
	ret = otp_put (db, &ctx);
    otp_db_close (db);
    return ret;
}

/*
 * Print otp entries for one or all users
 */

static int
list_otps (int argc, char **argv, char *user)
{
    void *db;
    struct passwd *pw;

    db = otp_db_open ();
    if(db == NULL)
	errx (1, "otp_db_open failed");

    if (user)
	print_otp_entry_for_name(db, user);
    else
	/* scans all users... so as to get a deterministic order */
	while ((pw = getpwent()))
	    print_otp_entry_for_name(db, pw->pw_name);

    otp_db_close (db);
    return 0;
}

int
main (int argc, char **argv)
{
    int defaultp = 0;
    int uid = getuid();
    OtpAlgorithm *alg = otp_find_alg (OTP_ALG_DEFAULT);
    int optind = 0;

    setprogname (argv[0]);
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    if(deletep && uid != 0)
	errx (1, "Only root can delete OTPs");
    if(alg_string) {
	alg = otp_find_alg (alg_string);
	if (alg == NULL)
	    errx (1, "Unknown algorithm: %s", alg_string);
    }
    if (user && uid != 0)
	errx (1, "Only root can use `-u'");
    argc -= optind;
    argv += optind;

    if (!(listp || deletep || renewp || openp))
	defaultp = 1;

    if ( listp + deletep + renewp + defaultp + openp != 1)
	usage(1); /* one of -d or -l or -r or none */

    if(deletep || openp || listp) {
	if(argc != 0)
	    errx(1, "delete, open, and list requires no arguments");
    } else {
	if(argc != 2)
	    errx(1, "setup, and renew requires `num', and `seed'");
    }
    if (listp)
	return list_otps (argc, argv, user);

    if (user == NULL) {
	struct passwd *pwd;

	pwd = k_getpwuid(uid);
	if (pwd == NULL)
	    err (1, "You don't exist");
	user = pwd->pw_name;
    }

    /*
     * users other that root must provide the next OTP to update the sequence.
     * it avoids someone to use a pending session to change an OTP sequence.
     * see RFC 1938/8.0.
     */
    if (uid != 0 && (defaultp || renewp)) {
	if (!has_an_otp(user)) {
	    errx (1, "Only root can set an initial OTP");
	} else { /* Check the next OTP (RFC 1938/8.0: SHOULD) */
	    if (verify_user_otp(user) != 0) {
		errx (1, "User authentification failed");
	    }
	}
    }

    if (deletep)
	return delete_otp (argc, argv, user);
    else if (renewp)
	return renew (argc, argv, alg, user);
    else if (openp)
	return open_otp (argc, argv, user);
    else
	return set (argc, argv, alg, user);
}
