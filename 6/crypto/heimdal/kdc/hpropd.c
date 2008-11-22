/*
 * Copyright (c) 1997-2002 Kungliga Tekniska Högskolan
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

#include "hprop.h"

RCSID("$Id: hpropd.c,v 1.36 2003/04/16 15:46:32 lha Exp $");

#ifdef KRB4
static des_cblock mkey4;
static des_key_schedule msched4;

static char *
time2str(time_t t)
{
    static char buf[128];
    strftime(buf, sizeof(buf), "%Y%m%d%H%M", gmtime(&t));
    return buf;
}

static int 
dump_krb4(krb5_context context, hdb_entry *ent, int fd)
{
    char name[ANAME_SZ];  
    char instance[INST_SZ];
    char realm[REALM_SZ];
    char buf[1024];
    char *p;
    int i;
    int ret;
    char *princ_name;
    Event *modifier;
    krb5_realm *realms;
    int cmp;
  
    ret = krb5_524_conv_principal(context, ent->principal,
				  name, instance, realm);
    if (ret) {
	krb5_unparse_name(context, ent->principal, &princ_name);
	krb5_warn(context, ret, "%s", princ_name);
	free(princ_name);
	return -1;
    }

    ret = krb5_get_default_realms (context, &realms);
    if (ret) {
	krb5_warn(context, ret, "krb5_get_default_realms");
	return -1;
    }

    cmp = strcmp (realms[0], ent->principal->realm);
    krb5_free_host_realm (context, realms);
    if (cmp != 0)
        return -1;

    snprintf (buf, sizeof(buf), "%s %s ", name,
	      (strlen(instance) != 0) ? instance : "*");

    if (ent->max_life) { 
	asprintf(&p, "%d", krb_time_to_life(0, *ent->max_life));
	strlcat(buf, p, sizeof(buf));
	free(p);
    } else 
	strlcat(buf, "255", sizeof(buf)); 
    strlcat(buf, " ", sizeof(buf));

    i = 0;
    while (i < ent->keys.len &&
	   ent->keys.val[i].key.keytype != KEYTYPE_DES)
	++i;

    if (i == ent->keys.len) {
	krb5_warnx(context, "No DES key for %s.%s", name, instance);
	return -1;
    }

    if (ent->keys.val[i].mkvno)
	asprintf(&p, "%d ", *ent->keys.val[i].mkvno);
    else
	asprintf(&p, "%d ", 1);
    strlcat(buf, p, sizeof(buf));
    free(p);

    asprintf(&p, "%d ", ent->kvno);
    strlcat(buf, p, sizeof(buf));
    free(p);

    asprintf(&p, "%d ", 0); /* Attributes are always 0*/  
    strlcat(buf, p, sizeof(buf));
    free(p);

    { 
	u_int32_t *key = ent->keys.val[i].key.keyvalue.data;
	kdb_encrypt_key((des_cblock*)key, (des_cblock*)key,
			&mkey4, msched4, DES_ENCRYPT);
	asprintf(&p, "%x %x ", (int)htonl(*key), (int)htonl(*(key+1)));
	strlcat(buf, p, sizeof(buf));
	free(p);
    }
 
    if (ent->valid_end == NULL)
	strlcat(buf, time2str(60*60*24*365*50), sizeof(buf)); /*no expiration*/
    else
	strlcat(buf, time2str(*ent->valid_end), sizeof(buf));
    strlcat(buf, " ", sizeof(buf));

    if (ent->modified_by == NULL) 
	modifier = &ent->created_by;
    else  
	modifier = ent->modified_by;
    
    ret = krb5_524_conv_principal(context, modifier->principal,
				  name, instance, realm);
    if (ret) { 
	krb5_unparse_name(context, modifier->principal, &princ_name);
	krb5_warn(context, ret, "%s", princ_name);
	free(princ_name);
	return -1;
    } 
    asprintf(&p, "%s %s %s\n", time2str(modifier->time), 
	     (strlen(name) != 0) ? name : "*", 
	     (strlen(instance) != 0) ? instance : "*");
    strlcat(buf, p, sizeof(buf));
    free(p);

    ret = write(fd, buf, strlen(buf));
    if (ret == -1)
	krb5_warnx(context, "write");
    return 0;
}
#endif /* KRB4 */

static int inetd_flag = -1;
static int help_flag;
static int version_flag;
static int print_dump;
static const char *database = HDB_DEFAULT_DB;
static int from_stdin;
static char *local_realm;
#ifdef KRB4
static int v4dump;
#endif
static char *ktname = NULL;

struct getargs args[] = {
    { "database", 'd', arg_string, &database, "database", "file" },
    { "stdin",    'n', arg_flag, &from_stdin, "read from stdin" },
    { "print",	    0, arg_flag, &print_dump, "print dump to stdout" },
    { "inetd",	   'i',	arg_negative_flag,	&inetd_flag,
      "Not started from inetd" },
    { "keytab",   'k',	arg_string, &ktname,	"keytab to use for authentication", "keytab" },
    { "realm",   'r',	arg_string, &local_realm, "realm to use" },
#ifdef KRB4
    { "v4dump",       '4',  arg_flag, &v4dump, "create v4 type DB" },
#endif
    { "version",    0, arg_flag, &version_flag, NULL, NULL },
    { "help",    'h',  arg_flag, &help_flag, NULL, NULL}
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_auth_context ac = NULL;
    krb5_principal c1, c2;
    krb5_authenticator authent;
    krb5_keytab keytab;
    int fd;
    HDB *db;
    int optind = 0;
    char *tmp_db;
    krb5_log_facility *fac;
    int nprincs;
#ifdef KRB4
    int e;
    int fd_out = -1;
#endif

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if(ret)
	exit(1);

    ret = krb5_openlog(context, "hpropd", &fac);
    if(ret)
	;
    krb5_set_warn_dest(context, fac);
  
    if(getarg(args, num_args, argc, argv, &optind))
	usage(1);

#ifdef KRB4
    if (v4dump && database == HDB_DEFAULT_DB)
       database = "/var/kerberos/524_dump";
#endif /* KRB4 */

    if(local_realm != NULL)
	krb5_set_default_realm(context, local_realm);
    
    if(help_flag)
	usage(0);
    if(version_flag) {
	print_version(NULL);
	exit(0);
    }
    
    argc -= optind;
    argv += optind;

    if (argc != 0)
	usage(1);

    if(from_stdin)
	fd = STDIN_FILENO;
    else {
	struct sockaddr_storage ss;
	struct sockaddr *sa = (struct sockaddr *)&ss;
	socklen_t sin_len = sizeof(ss);
	char addr_name[256];
	krb5_ticket *ticket;
	char *server;

	fd = STDIN_FILENO;
	if (inetd_flag == -1) {
	    if (getpeername (fd, sa, &sin_len) < 0)
		inetd_flag = 0;
	    else
		inetd_flag = 1;
	}
	if (!inetd_flag) {
	    mini_inetd (krb5_getportbyname (context, "hprop", "tcp",
					    HPROP_PORT));
	}
	sin_len = sizeof(ss);
	if(getpeername(fd, sa, &sin_len) < 0)
	    krb5_err(context, 1, errno, "getpeername");

	if (inet_ntop(sa->sa_family,
		      socket_get_address (sa),
		      addr_name,
		      sizeof(addr_name)) == NULL)
	    strlcpy (addr_name, "unknown address",
			     sizeof(addr_name));

	krb5_log(context, fac, 0, "Connection from %s", addr_name);
    
	ret = krb5_kt_register(context, &hdb_kt_ops);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_kt_register");

	if (ktname != NULL) {
	    ret = krb5_kt_resolve(context, ktname, &keytab);
	    if (ret)
		krb5_err (context, 1, ret, "krb5_kt_resolve %s", ktname);
	} else {
	    ret = krb5_kt_default (context, &keytab);
	    if (ret)
		krb5_err (context, 1, ret, "krb5_kt_default");
	}

	ret = krb5_recvauth(context, &ac, &fd, HPROP_VERSION, NULL,
			    0, keytab, &ticket);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_recvauth");
	
	ret = krb5_unparse_name(context, ticket->server, &server);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_unparse_name");
	if (strncmp(server, "hprop/", 5) != 0)
	    krb5_errx(context, 1, "ticket not for hprop (%s)", server);

	free(server);
	krb5_free_ticket (context, ticket);

	ret = krb5_auth_con_getauthenticator(context, ac, &authent);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_auth_con_getauthenticator");
	
	ret = krb5_make_principal(context, &c1, NULL, "kadmin", "hprop", NULL);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_make_principal");
	principalname2krb5_principal(&c2, authent->cname, authent->crealm);
	if(!krb5_principal_compare(context, c1, c2)) {
	    char *s;
	    krb5_unparse_name(context, c2, &s);
	    krb5_errx(context, 1, "Unauthorized connection from %s", s);
	}
	krb5_free_principal(context, c1);
	krb5_free_principal(context, c2);

	ret = krb5_kt_close(context, keytab);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_kt_close");
    }
    
    if(!print_dump) {
	asprintf(&tmp_db, "%s~", database);
#ifdef KRB4
	if (v4dump) {
	    fd_out = open(tmp_db, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	    if (fd_out == -1)
		krb5_errx(context, 1, "%s", strerror(errno));
	}
	else
#endif /* KRB4 */
	{
	    ret = hdb_create(context, &db, tmp_db);
	    if(ret)
		krb5_err(context, 1, ret, "hdb_create(%s)", tmp_db);
	    ret = db->open(context, db, O_RDWR | O_CREAT | O_TRUNC, 0600);
	    if(ret)
		krb5_err(context, 1, ret, "hdb_open(%s)", tmp_db);
	}
    }

#ifdef KRB4
    if (v4dump) {
	e = kdb_get_master_key(0, &mkey4, msched4);
	if(e)
	    krb5_errx(context, 1, "kdb_get_master_key: %s",
		      krb_get_err_text(e));
    }
#endif /* KRB4 */

    nprincs = 0;
    while(1){
	krb5_data data;
	hdb_entry entry;

	if(from_stdin) {
	    ret = krb5_read_message(context, &fd, &data);
	    if(ret != 0 && ret != HEIM_ERR_EOF)
		krb5_err(context, 1, ret, "krb5_read_message");
	} else {
	    ret = krb5_read_priv_message(context, ac, &fd, &data);
	    if(ret)
		krb5_err(context, 1, ret, "krb5_read_priv_message");
	}

	if(ret == HEIM_ERR_EOF || data.length == 0) {
	    if(!from_stdin) {
		data.data = NULL;
		data.length = 0;
		krb5_write_priv_message(context, ac, &fd, &data);
	    }
	    if(!print_dump) {
#ifdef KRB4
		if (v4dump) {
		    ret = rename(tmp_db, database);
		    if (ret)
			krb5_errx(context, 1, "rename");
		    ret = close(fd_out);
		    if (ret)
			krb5_errx(context, 1, "close");
		} else
#endif /* KRB4 */
		{
		    ret = db->rename(context, db, database);
		    if(ret)
			krb5_err(context, 1, ret, "db_rename");
		    ret = db->close(context, db);
		    if(ret)
			krb5_err(context, 1, ret, "db_close");
		}
	    }
	    break;
	}
	ret = hdb_value2entry(context, &data, &entry);
	if(ret)
	    krb5_err(context, 1, ret, "hdb_value2entry");
	if(print_dump)
	    hdb_print_entry(context, db, &entry, stdout);
	else {
#ifdef KRB4
	    if (v4dump) {
		ret = dump_krb4(context, &entry, fd_out);
		if(!ret) nprincs++;
	    }
	    else
#endif /* KRB4 */
	    {
		ret = db->store(context, db, 0, &entry);
		if(ret == HDB_ERR_EXISTS) {
		    char *s;
		    krb5_unparse_name(context, entry.principal, &s);
		    krb5_warnx(context, "Entry exists: %s", s);
		    free(s);
		} else if(ret) 
		    krb5_err(context, 1, ret, "db_store");
		else
		    nprincs++;
	    }
	}
	hdb_free_entry(context, &entry);
    }
    if (!print_dump)
	krb5_log(context, fac, 0, "Received %d principals", nprincs);
    exit(0);
}
