/*
 * Copyright (c) 1997 - 2001, 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: change.c,v 1.5 2003/04/01 15:04:49 lha Exp $");

static void
change_entry (krb5_context context, krb5_keytab keytab,
	      krb5_principal principal, krb5_kvno kvno,
	      const char *realm, const char *admin_server, int server_port)
{
    krb5_error_code ret;
    kadm5_config_params conf;
    void *kadm_handle;
    char *client_name;
    krb5_keyblock *keys;
    int num_keys;
    int i;

    ret = krb5_unparse_name (context, principal, &client_name);
    if (ret) {
	krb5_warn (context, ret, "krb5_unparse_name");
	return;
    }

    memset (&conf, 0, sizeof(conf));

    if(realm)
	conf.realm = (char *)realm;
    else
	conf.realm = *krb5_princ_realm (context, principal);
    conf.mask |= KADM5_CONFIG_REALM;
    
    if (admin_server) {
	conf.admin_server = (char *)admin_server;
	conf.mask |= KADM5_CONFIG_ADMIN_SERVER;
    }

    if (server_port) {
	conf.kadmind_port = htons(server_port);
	conf.mask |= KADM5_CONFIG_KADMIND_PORT;
    }

    ret = kadm5_init_with_skey_ctx (context,
				    client_name,
				    keytab_string,
				    KADM5_ADMIN_SERVICE,
				    &conf, 0, 0,
				    &kadm_handle);
    free (client_name);
    if (ret) {
	krb5_warn (context, ret, "kadm5_c_init_with_skey_ctx");
	return;
    }
    ret = kadm5_randkey_principal (kadm_handle, principal, &keys, &num_keys);
    kadm5_destroy (kadm_handle);
    if (ret) {
	krb5_warn(context, ret, "kadm5_randkey_principal");
	return;
    }
    for (i = 0; i < num_keys; ++i) {
	krb5_keytab_entry new_entry;

	new_entry.principal = principal;
	new_entry.timestamp = time (NULL);
	new_entry.vno = kvno + 1;
	new_entry.keyblock  = keys[i];

	ret = krb5_kt_add_entry (context, keytab, &new_entry);
	if (ret)
	    krb5_warn (context, ret, "krb5_kt_add_entry");
	krb5_free_keyblock_contents (context, &keys[i]);
    }
}

/*
 * loop over all the entries in the keytab (or those given) and change
 * their keys, writing the new keys
 */

struct change_set {
    krb5_principal principal;
    krb5_kvno kvno;
};

int
kt_change (int argc, char **argv)
{
    krb5_error_code ret;
    krb5_keytab keytab;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    char *realm = NULL;
    char *admin_server = NULL;
    int server_port = 0;
    int help_flag = 0;
    int optind = 0;
    int i, j, max;
    struct change_set *changeset;
    
    struct getargs args[] = {
	{ "realm",	'r',	arg_string,   NULL, 
	  "realm to use", "realm" 
	},
	{ "admin-server",	'a',	arg_string, NULL,
	  "server to contact", "host" 
	},
	{ "server-port",	's',	arg_integer, NULL,
	  "port to contact", "port number" 
	},
	{ "help",		'h',	arg_flag,    NULL }
    };

    args[0].value = &realm;
    args[1].value = &admin_server;
    args[2].value = &server_port;
    args[3].value = &help_flag;

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optind)
       || help_flag) {
	arg_printusage(args, sizeof(args) / sizeof(args[0]), 
		       "ktutil change", "principal...");
	return 1;
    }
    
    if((keytab = ktutil_open_keytab()) == NULL)
	return 1;

    j = 0;
    max = 0;
    changeset = NULL;

    ret = krb5_kt_start_seq_get(context, keytab, &cursor);
    if(ret){
	krb5_warn(context, ret, "krb5_kt_start_seq_get %s", keytab_string);
	goto out;
    }

    while((ret = krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0) {
	int add = 0;

	for (i = 0; i < j; ++i) {
	    if (krb5_principal_compare (context, changeset[i].principal,
					entry.principal)) {
		if (changeset[i].kvno < entry.vno)
		    changeset[i].kvno = entry.vno;
		break;
	    }
	}
	if (i < j)
	    continue;

	if (optind == argc) {
	    add = 1;
	} else {
	    for (i = optind; i < argc; ++i) {
		krb5_principal princ;

		ret = krb5_parse_name (context, argv[i], &princ);
		if (ret) {
		    krb5_warn (context, ret, "krb5_parse_name %s", argv[i]);
		    continue;
		}
		if (krb5_principal_compare (context, princ, entry.principal))
		    add = 1;

		krb5_free_principal (context, princ);
	    }
	}

	if (add) {
	    if (j >= max) {
		void *tmp;

		max = max(max * 2, 1);
		tmp = realloc (changeset, max * sizeof(*changeset));
		if (tmp == NULL) {
		    krb5_kt_free_entry (context, &entry);
		    krb5_warnx (context, "realloc: out of memory");
		    ret = ENOMEM;
		    break;
		}
		changeset = tmp;
	    }
	    ret = krb5_copy_principal (context, entry.principal,
				       &changeset[j].principal);
	    if (ret) {
		krb5_warn (context, ret, "krb5_copy_principal");
		krb5_kt_free_entry (context, &entry);
		break;
	    }
	    changeset[j].kvno = entry.vno;
	    ++j;
	}
	krb5_kt_free_entry (context, &entry);
    }

    if (ret == KRB5_KT_END) {
	for (i = 0; i < j; i++) {
	    if (verbose_flag) {
		char *client_name;

		ret = krb5_unparse_name (context, changeset[i].principal, 
					 &client_name);
		if (ret) {
		    krb5_warn (context, ret, "krb5_unparse_name");
		} else {
		    printf("Changing %s kvno %d\n", 
			   client_name, changeset[i].kvno);
		    free(client_name);
		}
	    }
	    change_entry (context, keytab, 
			  changeset[i].principal, changeset[i].kvno,
			  realm, admin_server, server_port);
	}
    }
    for (i = 0; i < j; i++)
	krb5_free_principal (context, changeset[i].principal);
    free (changeset);

    ret = krb5_kt_end_seq_get(context, keytab, &cursor);
 out:
    krb5_kt_close(context, keytab);
    return 0;
}
