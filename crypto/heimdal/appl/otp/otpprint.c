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

RCSID("$Id: otpprint.c,v 1.14 2001/02/20 01:44:46 assar Exp $");

static int extendedp;
static int count = 10;
static int hexp;
static char* alg_string;
static int version_flag;
static int help_flag;

struct getargs args[] = {
    { "extended", 'e', arg_flag, &extendedp, "print keys in extended format" },
    { "count", 'n', arg_integer, &count, "number of keys to print" },
    { "hexadecimal", 'h', arg_flag, &hexp, "output in hexadecimal" },
    { "hash", 'f', arg_string, &alg_string, 
      "hash algorithm (md4, md5, or sha)", "algorithm"},
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};

int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int code)
{
    arg_printusage(args, num_args, NULL, "num seed");
    exit(code);
}

static int
print (int argc,
       char **argv,
       int count,
       OtpAlgorithm *alg,
       void (*print_fn)(OtpKey, char *, size_t))
{
  char pw[64];
  OtpKey key;
  int n;
  int i;
  char *seed;

  if (argc != 2)
      usage (1);
  n = atoi(argv[0]);
  seed = argv[1];
  if (des_read_pw_string (pw, sizeof(pw), "Pass-phrase: ", 0))
    return 1;
  alg->init (key, pw, seed);
  for (i = 0; i < n; ++i) {
    char s[64];

    alg->next (key);
    if (i >= n - count) {
      (*print_fn)(key, s, sizeof(s));
      printf ("%d: %s\n", i + 1, s);
    }
  }
  return 0;
}

int
main (int argc, char **argv)
{
    int optind = 0;
    void (*fn)(OtpKey, char *, size_t);
    OtpAlgorithm *alg = otp_find_alg (OTP_ALG_DEFAULT);

    setprogname (argv[0]);
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    if(alg_string) {
	alg = otp_find_alg (alg_string);
	if (alg == NULL)
	    errx(1, "Unknown algorithm: %s", alg_string);
    }
    argc -= optind;
    argv += optind;

    if (hexp) {
	if (extendedp)
	    fn = otp_print_hex_extended;
	else
	    fn = otp_print_hex;
    } else {
	if (extendedp)
	    fn = otp_print_stddict_extended;
	else
	    fn = otp_print_stddict;
    }

    return print (argc, argv, count, alg, fn);
}
