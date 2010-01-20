/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <gssapi.h>
#include <err.h>
#include <roken.h>
#include <getarg.h>
#include <rtbl.h>
#include <gss-commands.h>
#include <krb5.h>

RCSID("$Id: gss.c 19922 2007-01-16 09:32:03Z lha $");

static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "service@host");
    exit (ret);
}

#define COL_OID		"OID"
#define COL_NAME	"Name"

int
supported_mechanisms(void *argptr, int argc, char **argv)
{
    OM_uint32 maj_stat, min_stat;
    gss_OID_set mechs;
    rtbl_t ct;
    size_t i;

    maj_stat = gss_indicate_mechs(&min_stat, &mechs);
    if (maj_stat != GSS_S_COMPLETE)
	errx(1, "gss_indicate_mechs failed");

    printf("Supported mechanisms:\n");

    ct = rtbl_create();
    if (ct == NULL)
	errx(1, "rtbl_create");

    rtbl_set_separator(ct, "  ");
    rtbl_add_column(ct, COL_OID, 0);
    rtbl_add_column(ct, COL_NAME, 0);

    for (i = 0; i < mechs->count; i++) {
	gss_buffer_desc name;

	maj_stat = gss_oid_to_str(&min_stat, &mechs->elements[i], &name);
	if (maj_stat != GSS_S_COMPLETE)
	    errx(1, "gss_oid_to_str failed");

	rtbl_add_column_entryv(ct, COL_OID, "%.*s",
			       (int)name.length, (char *)name.value);
	gss_release_buffer(&min_stat, &name);

	if (gss_oid_equal(&mechs->elements[i], GSS_KRB5_MECHANISM))
	    rtbl_add_column_entry(ct, COL_NAME, "Kerberos 5");
	else if (gss_oid_equal(&mechs->elements[i], GSS_SPNEGO_MECHANISM))
	    rtbl_add_column_entry(ct, COL_NAME, "SPNEGO");
	else if (gss_oid_equal(&mechs->elements[i], GSS_NTLM_MECHANISM))
	    rtbl_add_column_entry(ct, COL_NAME, "NTLM");
    }
    gss_release_oid_set(&min_stat, &mechs);

    rtbl_format(ct, stdout);
    rtbl_destroy(ct);

    return 0;
}

#if 0
/*
 *
 */

#define DOVEDOT_MAJOR_VERSION 1
#define DOVEDOT_MINOR_VERSION 0

/*
	S: MECH mech mech-parameters
	S: MECH mech mech-parameters
	S: VERSION major minor
	S: CPID pid
	S: CUID pid
	S: ...
	S: DONE
	C: VERSION major minor
	C: CPID pid

	C: AUTH id method service= resp=
	C: CONT id message

	S: OK id user=
	S: FAIL id reason=
	S: CONTINUE id message
*/

int
dovecot_server(void *argptr, int argc, char **argv)
{
    krb5_storage *sp;
    int fd = 0;

    sp = krb5_storage_from_fd(fd);
    if (sp == NULL)
	errx(1, "krb5_storage_from_fd");

    krb5_store_stringnl(sp, "MECH\tGSSAPI");
    krb5_store_stringnl(sp, "VERSION\t1\t0");
    krb5_store_stringnl(sp, "DONE");

    while (1) {
	char *cmd;
	if (krb5_ret_stringnl(sp, &cmd) != 0)
	    break;
	printf("cmd: %s\n", cmd);
	free(cmd);
    }
    return 0;
}
#endif

/*
 *
 */

int
help(void *opt, int argc, char **argv)
{
    sl_slc_help(commands, argc, argv);
    return 0;
}

int
main(int argc, char **argv)
{
    int optidx = 0;

    setprogname(argv[0]);
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);
    
    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc == 0) {
	help(NULL, argc, argv);
	return 1;
    }

    return sl_command (commands, argc, argv);
}
