/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#include "iprop.h"

RCSID("$Id: truncate_log.c,v 1.1 2000/07/24 04:27:06 assar Exp $");

static char *realm;
static int version_flag;
static int help_flag;

static struct getargs args[] = {
    { "realm", 'r', arg_string, &realm },
    { "version", 0, arg_flag, &version_flag },
    { "help", 0, arg_flag, &help_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    void *kadm_handle;
    kadm5_server_context *server_context;
    kadm5_config_params conf;

    krb5_program_setup(&context, argc, argv, args, num_args, NULL);
    
    if(help_flag)
	krb5_std_usage(0, args, num_args);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    memset(&conf, 0, sizeof(conf));
    if(realm) {
	conf.mask |= KADM5_CONFIG_REALM;
	conf.realm = realm;
    }

    ret = kadm5_init_with_password_ctx (context,
					KADM5_ADMIN_SERVICE,
					NULL,
					KADM5_ADMIN_SERVICE,
					&conf, 0, 0, 
					&kadm_handle);
    if (ret)
	krb5_err (context, 1, ret, "kadm5_init_with_password_ctx");

    server_context = (kadm5_server_context *)kadm_handle;

    ret = kadm5_log_truncate (server_context);
	krb5_err (context, 1, ret, "kadm5_log_truncate");
    return 0;
}
