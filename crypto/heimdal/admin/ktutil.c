/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "ktutil_locl.h"
#include <err.h>

RCSID("$Id: ktutil.c,v 1.30 2001/01/25 12:44:37 assar Exp $");

static int help_flag;
static int version_flag;
int verbose_flag;
char *keytab_string; 

static char keytab_buf[256];

static int help(int argc, char **argv);

static SL_cmd cmds[] = {
    { "add", 		kt_add,		"add",
      "adds key to keytab" },
    { "change",		kt_change,	"change [principal...]",
      "get new key for principals (all)" },
    { "copy",		kt_copy,	"copy src dst",
      "copy one keytab to another" },
    { "get", 		kt_get,		"get [principal...]",
      "create key in database and add to keytab" },
    { "list",		kt_list,	"list",
      "shows contents of a keytab" },
    { "purge",		kt_purge,	"purge",
      "remove old and superceeded entries" },
    { "remove", 	kt_remove,	"remove",
      "remove key from keytab" },
    { "srvconvert",	srvconv,	"srvconvert [flags]",
      "convert v4 srvtab to keytab" },
    { "srv2keytab" },
    { "srvcreate",	srvcreate,	"srvcreate [flags]",
      "convert keytab to v4 srvtab" },
    { "key2srvtab" },
    { "help",		help,		"help",			"" },
    { NULL, 	NULL,		NULL, 			NULL }
};

static struct getargs args[] = {
    { 
	"version",
	0,
	arg_flag,
	&version_flag,
	NULL,
	NULL 
    },
    { 
	"help",	    
	'h',   
	arg_flag, 
	&help_flag, 
	NULL, 
	NULL
    },
    { 
	"keytab",	    
	'k',   
	arg_string, 
	&keytab_string, 
	"keytab", 
	"keytab to operate on" 
    },
    {
	"verbose",
	'v',
	arg_flag,
	&verbose_flag,
	"verbose",
	"run verbosely"
    }
};

static int num_args = sizeof(args) / sizeof(args[0]);

krb5_context context;
krb5_keytab keytab;

static int
help(int argc, char **argv)
{
    sl_help(cmds, argc, argv);
    return 0;
}

static void
usage(int status)
{
    arg_printusage(args, num_args, NULL, "command");
    exit(status);
}

int
main(int argc, char **argv)
{
    int optind = 0;
    krb5_error_code ret;
    set_progname(argv[0]);
    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    argc -= optind;
    argv += optind;
    if(argc == 0)
	usage(1);
    if(keytab_string) {
	ret = krb5_kt_resolve(context, keytab_string, &keytab);
    } else {
	if(krb5_kt_default_name (context, keytab_buf, sizeof(keytab_buf)))
	    strlcpy (keytab_buf, "unknown", sizeof(keytab_buf));
	keytab_string = keytab_buf;

	ret = krb5_kt_default(context, &keytab);
    }
    if(ret)
	krb5_err(context, 1, ret, "resolving keytab");
    ret = sl_command(cmds, argc, argv);
    if(ret == -1)
	krb5_warnx (context, "unrecognized command: %s", argv[0]);
    krb5_kt_close(context, keytab);
    return ret;
}
