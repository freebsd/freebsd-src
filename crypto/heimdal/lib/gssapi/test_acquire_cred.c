/*
 * Copyright (c) 2003-2007 Kungliga Tekniska Högskolan
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gssapi.h>
#include <err.h>
#include <roken.h>
#include <getarg.h>

#include "test_common.h"

RCSID("$Id: test_acquire_cred.c 22129 2007-12-04 01:13:13Z lha $");

static void
print_time(OM_uint32 time_rec)
{
    if (time_rec == GSS_C_INDEFINITE) {
	printf("cred never expire\n");
    } else {
	time_t t = time_rec + time(NULL);
	printf("expiration time: %s", ctime(&t));
    }
}

#if 0

static void
test_add(gss_cred_id_t cred_handle)
{
    OM_uint32 major_status, minor_status;
    gss_cred_id_t copy_cred;
    OM_uint32 time_rec;

    major_status = gss_add_cred (&minor_status,
				 cred_handle,
				 GSS_C_NO_NAME,
				 GSS_KRB5_MECHANISM,
				 GSS_C_INITIATE,
				 0,
				 0,
				 &copy_cred,
				 NULL,
				 &time_rec,
				 NULL);
			    
    if (GSS_ERROR(major_status))
	errx(1, "add_cred failed");

    print_time(time_rec);

    major_status = gss_release_cred(&minor_status,
				    &copy_cred);
    if (GSS_ERROR(major_status))
	errx(1, "release_cred failed");
}

static void
copy_cred(void)
{
    OM_uint32 major_status, minor_status;
    gss_cred_id_t cred_handle;
    OM_uint32 time_rec;

    major_status = gss_acquire_cred(&minor_status, 
				    GSS_C_NO_NAME,
				    0,
				    NULL,
				    GSS_C_INITIATE,
				    &cred_handle,
				    NULL,
				    &time_rec);
    if (GSS_ERROR(major_status))
	errx(1, "acquire_cred failed");
	
    print_time(time_rec);

    test_add(cred_handle);
    test_add(cred_handle);
    test_add(cred_handle);

    major_status = gss_release_cred(&minor_status,
				    &cred_handle);
    if (GSS_ERROR(major_status))
	errx(1, "release_cred failed");
}
#endif

static void
acquire_cred_service(const char *service,
		     gss_OID nametype,
		     int flags)
{
    OM_uint32 major_status, minor_status;
    gss_cred_id_t cred_handle;
    OM_uint32 time_rec;
    gss_buffer_desc name_buffer;
    gss_name_t name = GSS_C_NO_NAME;

    if (service) {
	name_buffer.value = rk_UNCONST(service);
	name_buffer.length = strlen(service);
	
	major_status = gss_import_name(&minor_status,
				       &name_buffer,
				       nametype,
				       &name);
	if (GSS_ERROR(major_status))
	    errx(1, "import_name failed");
    }

    major_status = gss_acquire_cred(&minor_status, 
				    name,
				    0,
				    NULL,
				    flags,
				    &cred_handle,
				    NULL,
				    &time_rec);
    if (GSS_ERROR(major_status)) {
	warnx("acquire_cred failed: %s", 
	     gssapi_err(major_status, minor_status, GSS_C_NO_OID));
    } else {    
	print_time(time_rec);
	gss_release_cred(&minor_status, &cred_handle);
    }

    if (name != GSS_C_NO_NAME)
	gss_release_name(&minor_status, &name);

    if (GSS_ERROR(major_status))
	exit(1);
}

static int version_flag = 0;
static int help_flag	= 0;
static char *acquire_name;
static char *acquire_type;
static char *name_type;
static char *ccache;

static struct getargs args[] = {
    {"acquire-name", 0,	arg_string,	&acquire_name, "name", NULL },
    {"acquire-type", 0,	arg_string,	&acquire_type, "type", NULL },
    {"ccache", 0,	arg_string,	&ccache, "name", NULL },
    {"name-type", 0,	arg_string,	&name_type, "type", NULL },
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int optidx = 0;
    OM_uint32 flag;
    gss_OID type;

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

    if (argc != 0)
	usage(1);

    if (acquire_type) {
	if (strcasecmp(acquire_type, "both") == 0)
	    flag = GSS_C_BOTH;
	else if (strcasecmp(acquire_type, "accept") == 0)
	    flag = GSS_C_ACCEPT;
	else if (strcasecmp(acquire_type, "initiate") == 0)
	    flag = GSS_C_INITIATE;
	else
	    errx(1, "unknown type %s", acquire_type);
    } else
	flag = GSS_C_ACCEPT;
	
    if (name_type) {
	if (strcasecmp("hostbased-service", name_type) == 0)
	    type = GSS_C_NT_HOSTBASED_SERVICE;
	else if (strcasecmp("user-name", name_type) == 0)
	    type = GSS_C_NT_USER_NAME;
	else
	    errx(1, "unknown name type %s", name_type);
    } else
	type = GSS_C_NT_HOSTBASED_SERVICE;

    if (ccache) {
	OM_uint32 major_status, minor_status;
	major_status = gss_krb5_ccache_name(&minor_status,
					    ccache, NULL);
	if (GSS_ERROR(major_status))
	    errx(1, "gss_krb5_ccache_name %s", 
		 gssapi_err(major_status, minor_status, GSS_C_NO_OID));
    }

    acquire_cred_service(acquire_name, type, flag);

    return 0;
}
