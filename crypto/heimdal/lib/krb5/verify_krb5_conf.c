/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
#include <getarg.h>
RCSID("$Id: verify_krb5_conf.c,v 1.3 1999/12/02 17:05:13 joda Exp $");

/* verify krb5.conf */

static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "[config-file]");
    exit (ret);
}

int
main(int argc, char **argv)
{
    const char *config_file = NULL;
    krb5_error_code ret;
    krb5_config_section *tmp_cf;
    unsigned lineno;
    char *error_message;
    int optind = 0;

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

    if (argc == 0) {
	config_file = getenv("KRB5_CONFIG");
	if (config_file == NULL)
	    config_file = krb5_config_file;
    } else if (argc == 1) {
	config_file = argv[0];
    } else {
	usage (1);
    }
    
    ret = krb5_config_parse_file_debug (config_file, &tmp_cf, &lineno,
					&error_message);
    if (ret == 0)
	return 0;
    fprintf (stderr, "%s:%u: %s\n", config_file, lineno, error_message);
    return 1;
}
