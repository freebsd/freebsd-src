/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
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

RCSID("$Id: hprop.c 21745 2007-07-31 16:11:25Z lha $");

static int version_flag;
static int help_flag;
static const char *ktname = HPROP_KEYTAB;
static const char *database;
static char *mkeyfile;
static int to_stdout;
static int verbose_flag;
static int encrypt_flag;
static int decrypt_flag;
static hdb_master_key mkey5;

static char *source_type;

static char *afs_cell;
static char *v4_realm;

static int kaspecials_flag;
static int ka_use_null_salt;

static char *local_realm=NULL;

static int
open_socket(krb5_context context, const char *hostname, const char *port)
{
    struct addrinfo *ai, *a;
    struct addrinfo hints;
    int error;

    memset (&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    error = getaddrinfo (hostname, port, &hints, &ai);
    if (error) {
	warnx ("%s: %s", hostname, gai_strerror(error));
	return -1;
    }
    
    for (a = ai; a != NULL; a = a->ai_next) {
	int s;

	s = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    warn ("connect(%s)", hostname);
	    close (s);
	    continue;
	}
	freeaddrinfo (ai);
	return s;
    }
    warnx ("failed to contact %s", hostname);
    freeaddrinfo (ai);
    return -1;
}

krb5_error_code
v5_prop(krb5_context context, HDB *db, hdb_entry_ex *entry, void *appdata)
{
    krb5_error_code ret;
    struct prop_data *pd = appdata;
    krb5_data data;

    if(encrypt_flag) {
	ret = hdb_seal_keys_mkey(context, &entry->entry, mkey5);
	if (ret) {
	    krb5_warn(context, ret, "hdb_seal_keys_mkey");
	    return ret;
	}
    }
    if(decrypt_flag) {
	ret = hdb_unseal_keys_mkey(context, &entry->entry, mkey5);
	if (ret) {
	    krb5_warn(context, ret, "hdb_unseal_keys_mkey");
	    return ret;
	}
    }	

    ret = hdb_entry2value(context, &entry->entry, &data);
    if(ret) {
	krb5_warn(context, ret, "hdb_entry2value");
	return ret;
    }

    if(to_stdout)
	ret = krb5_write_message(context, &pd->sock, &data);
    else
	ret = krb5_write_priv_message(context, pd->auth_context, 
				      &pd->sock, &data);
    krb5_data_free(&data);
    return ret;
}

int
v4_prop(void *arg, struct v4_principal *p)
{
    struct prop_data *pd = arg;
    hdb_entry_ex ent;
    krb5_error_code ret;

    memset(&ent, 0, sizeof(ent));

    ret = krb5_425_conv_principal(pd->context, p->name, p->instance, v4_realm,
				  &ent.entry.principal);
    if(ret) {
	krb5_warn(pd->context, ret,
		  "krb5_425_conv_principal %s.%s@%s",
		  p->name, p->instance, v4_realm);
	return 0;
    }

    if(verbose_flag) {
	char *s;
	krb5_unparse_name_short(pd->context, ent.entry.principal, &s);
	krb5_warnx(pd->context, "%s.%s -> %s", p->name, p->instance, s);
	free(s);
    }

    ent.entry.kvno = p->kvno;
    ent.entry.keys.len = 3;
    ent.entry.keys.val = malloc(ent.entry.keys.len * sizeof(*ent.entry.keys.val));
    if (ent.entry.keys.val == NULL)
	krb5_errx(pd->context, ENOMEM, "malloc");
    if(p->mkvno != -1) {
	ent.entry.keys.val[0].mkvno = malloc (sizeof(*ent.entry.keys.val[0].mkvno));
	if (ent.entry.keys.val[0].mkvno == NULL)
	    krb5_errx(pd->context, ENOMEM, "malloc");
	*(ent.entry.keys.val[0].mkvno) = p->mkvno;
    } else
	ent.entry.keys.val[0].mkvno = NULL;
    ent.entry.keys.val[0].salt = calloc(1, sizeof(*ent.entry.keys.val[0].salt));
    if (ent.entry.keys.val[0].salt == NULL)
	krb5_errx(pd->context, ENOMEM, "calloc");
    ent.entry.keys.val[0].salt->type = KRB5_PADATA_PW_SALT;
    ent.entry.keys.val[0].key.keytype = ETYPE_DES_CBC_MD5;
    krb5_data_alloc(&ent.entry.keys.val[0].key.keyvalue, DES_KEY_SZ);
    memcpy(ent.entry.keys.val[0].key.keyvalue.data, p->key, 8);

    copy_Key(&ent.entry.keys.val[0], &ent.entry.keys.val[1]);
    ent.entry.keys.val[1].key.keytype = ETYPE_DES_CBC_MD4;
    copy_Key(&ent.entry.keys.val[0], &ent.entry.keys.val[2]);
    ent.entry.keys.val[2].key.keytype = ETYPE_DES_CBC_CRC;

    {
	int life = _krb5_krb_life_to_time(0, p->max_life);
	if(life == NEVERDATE){
	    ent.entry.max_life = NULL;
	} else {
	    /* clean up lifetime a bit */
	    if(life > 86400)
		life = (life + 86399) / 86400 * 86400;
	    else if(life > 3600)
		life = (life + 3599) / 3600 * 3600;
	    ALLOC(ent.entry.max_life);
	    *ent.entry.max_life = life;
	}
    }

    ALLOC(ent.entry.valid_end);
    *ent.entry.valid_end = p->exp_date;

    ret = krb5_make_principal(pd->context, &ent.entry.created_by.principal,
			      v4_realm,
			      "kadmin",
			      "hprop",
			      NULL);
    if(ret){
	krb5_warn(pd->context, ret, "krb5_make_principal");
	ret = 0;
	goto out;
    }
    ent.entry.created_by.time = time(NULL);
    ALLOC(ent.entry.modified_by);
    ret = krb5_425_conv_principal(pd->context, p->mod_name, p->mod_instance, 
				  v4_realm, &ent.entry.modified_by->principal);
    if(ret){
	krb5_warn(pd->context, ret, "%s.%s@%s", p->name, p->instance, v4_realm);
	ent.entry.modified_by->principal = NULL;
	ret = 0;
	goto out;
    }
    ent.entry.modified_by->time = p->mod_date;

    ent.entry.flags.forwardable = 1;
    ent.entry.flags.renewable = 1;
    ent.entry.flags.proxiable = 1;
    ent.entry.flags.postdate = 1;
    ent.entry.flags.client = 1;
    ent.entry.flags.server = 1;
    
    /* special case password changing service */
    if(strcmp(p->name, "changepw") == 0 && 
       strcmp(p->instance, "kerberos") == 0) {
	ent.entry.flags.forwardable = 0;
	ent.entry.flags.renewable = 0;
	ent.entry.flags.proxiable = 0;
	ent.entry.flags.postdate = 0;
	ent.entry.flags.initial = 1;
	ent.entry.flags.change_pw = 1;
    }

    ret = v5_prop(pd->context, NULL, &ent, pd);

    if (strcmp (p->name, "krbtgt") == 0
	&& strcmp (v4_realm, p->instance) != 0) {
	krb5_free_principal (pd->context, ent.entry.principal);
	ret = krb5_425_conv_principal (pd->context, p->name,
				       v4_realm, p->instance,
				       &ent.entry.principal);
	if (ret == 0)
	    ret = v5_prop (pd->context, NULL, &ent, pd);
    }

  out:
    hdb_free_entry(pd->context, &ent);
    return ret;
}

#include "kadb.h"

/* read a `ka_entry' from `fd' at offset `pos' */
static void
read_block(krb5_context context, int fd, int32_t pos, void *buf, size_t len)
{
    krb5_error_code ret;
#ifdef HAVE_PREAD
    if((ret = pread(fd, buf, len, 64 + pos)) < 0)
	krb5_err(context, 1, errno, "pread(%u)", 64 + pos);
#else
    if(lseek(fd, 64 + pos, SEEK_SET) == (off_t)-1)
	krb5_err(context, 1, errno, "lseek(%u)", 64 + pos);
    ret = read(fd, buf, len);
    if(ret < 0)
	krb5_err(context, 1, errno, "read(%lu)", (unsigned long)len);
#endif
    if(ret != len)
	krb5_errx(context, 1, "read(%lu) = %u", (unsigned long)len, ret);
}

static int
ka_convert(struct prop_data *pd, int fd, struct ka_entry *ent)
{
    int32_t flags = ntohl(ent->flags);
    krb5_error_code ret;
    hdb_entry_ex hdb;

    if(!kaspecials_flag
       && (flags & KAFNORMAL) == 0) /* remove special entries */
	return 0;
    memset(&hdb, 0, sizeof(hdb));
    ret = krb5_425_conv_principal(pd->context, ent->name, ent->instance, 
				  v4_realm, &hdb.entry.principal);
    if(ret) {
	krb5_warn(pd->context, ret,
		  "krb5_425_conv_principal (%s.%s@%s)",
		  ent->name, ent->instance, v4_realm);
	return 0;
    }
    hdb.entry.kvno = ntohl(ent->kvno);
    hdb.entry.keys.len = 3;
    hdb.entry.keys.val = 
	malloc(hdb.entry.keys.len * sizeof(*hdb.entry.keys.val));
    if (hdb.entry.keys.val == NULL)
	krb5_errx(pd->context, ENOMEM, "malloc");
    hdb.entry.keys.val[0].mkvno = NULL;
    hdb.entry.keys.val[0].salt = calloc(1, sizeof(*hdb.entry.keys.val[0].salt));
    if (hdb.entry.keys.val[0].salt == NULL)
	krb5_errx(pd->context, ENOMEM, "calloc");
    if (ka_use_null_salt) {
	hdb.entry.keys.val[0].salt->type = hdb_pw_salt;
	hdb.entry.keys.val[0].salt->salt.data = NULL;
	hdb.entry.keys.val[0].salt->salt.length = 0;
    } else {
	hdb.entry.keys.val[0].salt->type = hdb_afs3_salt;
	hdb.entry.keys.val[0].salt->salt.data = strdup(afs_cell);
	if (hdb.entry.keys.val[0].salt->salt.data == NULL)
	    krb5_errx(pd->context, ENOMEM, "strdup");
	hdb.entry.keys.val[0].salt->salt.length = strlen(afs_cell);
    }
    
    hdb.entry.keys.val[0].key.keytype = ETYPE_DES_CBC_MD5;
    krb5_data_copy(&hdb.entry.keys.val[0].key.keyvalue,
		   ent->key,
		   sizeof(ent->key));
    copy_Key(&hdb.entry.keys.val[0], &hdb.entry.keys.val[1]);
    hdb.entry.keys.val[1].key.keytype = ETYPE_DES_CBC_MD4;
    copy_Key(&hdb.entry.keys.val[0], &hdb.entry.keys.val[2]);
    hdb.entry.keys.val[2].key.keytype = ETYPE_DES_CBC_CRC;

    ALLOC(hdb.entry.max_life);
    *hdb.entry.max_life = ntohl(ent->max_life);

    if(ntohl(ent->valid_end) != NEVERDATE && ntohl(ent->valid_end) != 0xffffffff) {
	ALLOC(hdb.entry.valid_end);
	*hdb.entry.valid_end = ntohl(ent->valid_end);
    }
    
    if (ntohl(ent->pw_change) != NEVERDATE && 
	ent->pw_expire != 255 &&
	ent->pw_expire != 0) {
	ALLOC(hdb.entry.pw_end);
	*hdb.entry.pw_end = ntohl(ent->pw_change)
	    + 24 * 60 * 60 * ent->pw_expire;
    }

    ret = krb5_make_principal(pd->context, &hdb.entry.created_by.principal,
			      v4_realm,
			      "kadmin",
			      "hprop",
			      NULL);
    hdb.entry.created_by.time = time(NULL);

    if(ent->mod_ptr){
	struct ka_entry mod;
	ALLOC(hdb.entry.modified_by);
	read_block(pd->context, fd, ntohl(ent->mod_ptr), &mod, sizeof(mod));
	
	krb5_425_conv_principal(pd->context, mod.name, mod.instance, v4_realm, 
				&hdb.entry.modified_by->principal);
	hdb.entry.modified_by->time = ntohl(ent->mod_time);
	memset(&mod, 0, sizeof(mod));
    }

    hdb.entry.flags.forwardable = 1;
    hdb.entry.flags.renewable = 1;
    hdb.entry.flags.proxiable = 1;
    hdb.entry.flags.postdate = 1;
    /* XXX - AFS 3.4a creates krbtgt.REALMOFCELL as NOTGS+NOSEAL */
    if (strcmp(ent->name, "krbtgt") == 0 &&
	(flags & (KAFNOTGS|KAFNOSEAL)) == (KAFNOTGS|KAFNOSEAL))
	flags &= ~(KAFNOTGS|KAFNOSEAL);

    hdb.entry.flags.client = (flags & KAFNOTGS) == 0;
    hdb.entry.flags.server = (flags & KAFNOSEAL) == 0;

    ret = v5_prop(pd->context, NULL, &hdb, pd);
    hdb_free_entry(pd->context, &hdb);
    return ret;
}

static int
ka_dump(struct prop_data *pd, const char *file)
{
    struct ka_header header;
    int i;
    int fd = open(file, O_RDONLY);

    if(fd < 0)
	krb5_err(pd->context, 1, errno, "open(%s)", file);
    read_block(pd->context, fd, 0, &header, sizeof(header));
    if(header.version1 != header.version2)
	krb5_errx(pd->context, 1, "Version mismatch in header: %ld/%ld",
		  (long)ntohl(header.version1), (long)ntohl(header.version2));
    if(ntohl(header.version1) != 5)
	krb5_errx(pd->context, 1, "Unknown database version %ld (expected 5)", 
		  (long)ntohl(header.version1));
    for(i = 0; i < ntohl(header.hashsize); i++){
	int32_t pos = ntohl(header.hash[i]);
	while(pos){
	    struct ka_entry ent;
	    read_block(pd->context, fd, pos, &ent, sizeof(ent));
	    ka_convert(pd, fd, &ent);
	    pos = ntohl(ent.next);
	}
    }
    return 0;
}



struct getargs args[] = {
    { "master-key", 'm', arg_string, &mkeyfile, "v5 master key file", "file" },
    { "database", 'd',	arg_string, &database, "database", "file" },
    { "source",   0,	arg_string, &source_type, "type of database to read", 
      "heimdal"
      "|mit-dump"
      "|krb4-dump"
      "|kaserver"
    },
      
    { "v4-realm", 'r',  arg_string, &v4_realm, "v4 realm to use" },
    { "cell",	  'c',  arg_string, &afs_cell, "name of AFS cell" },
    { "kaspecials", 'S', arg_flag,   &kaspecials_flag, "dump KASPECIAL keys"},
    { "keytab",   'k',	arg_string, &ktname, "keytab to use for authentication", "keytab" },
    { "v5-realm", 'R',  arg_string, &local_realm, "v5 realm to use" },
    { "decrypt",  'D',  arg_flag,   &decrypt_flag,   "decrypt keys" },
    { "encrypt",  'E',  arg_flag,   &encrypt_flag,   "encrypt keys" },
    { "stdout",	  'n',  arg_flag,   &to_stdout, "dump to stdout" },
    { "verbose",  'v',	arg_flag, &verbose_flag },
    { "version",   0,	arg_flag, &version_flag },
    { "help",     'h',	arg_flag, &help_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "[host[:port]] ...");
    exit (ret);
}

static void
get_creds(krb5_context context, krb5_ccache *cache)
{
    krb5_keytab keytab;
    krb5_principal client;
    krb5_error_code ret;
    krb5_get_init_creds_opt *init_opts;
    krb5_preauthtype preauth = KRB5_PADATA_ENC_TIMESTAMP;
    krb5_creds creds;
    
    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_register");

    ret = krb5_kt_resolve(context, ktname, &keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_resolve");
    
    ret = krb5_make_principal(context, &client, NULL, 
			      "kadmin", HPROP_NAME, NULL);
    if(ret) krb5_err(context, 1, ret, "krb5_make_principal");

    ret = krb5_get_init_creds_opt_alloc(context, &init_opts);
    if(ret) krb5_err(context, 1, ret, "krb5_get_init_creds_opt_alloc");
    krb5_get_init_creds_opt_set_preauth_list(init_opts, &preauth, 1);

    ret = krb5_get_init_creds_keytab(context, &creds, client, keytab, 0, NULL, init_opts);
    if(ret) krb5_err(context, 1, ret, "krb5_get_init_creds");

    krb5_get_init_creds_opt_free(context, init_opts);
    
    ret = krb5_kt_close(context, keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_close");
    
    ret = krb5_cc_gen_new(context, &krb5_mcc_ops, cache);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_initialize(context, *cache, client);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_initialize");

    krb5_free_principal(context, client);

    ret = krb5_cc_store_cred(context, *cache, &creds);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_store_cred");

    krb5_free_cred_contents(context, &creds);
}

enum hprop_source {
    HPROP_HEIMDAL = 1,
    HPROP_KRB4_DUMP,
    HPROP_KASERVER,
    HPROP_MIT_DUMP
};

#define IS_TYPE_V4(X) ((X) == HPROP_KRB4_DUMP || (X) == HPROP_KASERVER)

struct {
    int type;
    const char *name;
} types[] = {
    { HPROP_HEIMDAL,	"heimdal" },
    { HPROP_KRB4_DUMP,	"krb4-dump" },
    { HPROP_KASERVER, 	"kaserver" },
    { HPROP_MIT_DUMP,	"mit-dump" }
};

static int
parse_source_type(const char *s)
{
    int i;
    for(i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
	if(strstr(types[i].name, s) == types[i].name)
	    return types[i].type;
    }
    return 0;
}

static int
iterate (krb5_context context,
	 const char *database_name,
	 HDB *db,
	 int type,
	 struct prop_data *pd)
{
    int ret;

    switch(type) {
    case HPROP_KRB4_DUMP:
	ret = v4_prop_dump(pd, database_name);
	if(ret)
	    krb5_warnx(context, "v4_prop_dump: %s", 
		       krb5_get_err_text(context, ret));
	break;
    case HPROP_KASERVER:
	ret = ka_dump(pd, database_name);
	if(ret)
	    krb5_warn(context, ret, "ka_dump");
	break;
    case HPROP_MIT_DUMP:
	ret = mit_prop_dump(pd, database_name);
	if (ret)
	    krb5_warnx(context, "mit_prop_dump: %s",
		      krb5_get_err_text(context, ret));
	break;
    case HPROP_HEIMDAL:
	ret = hdb_foreach(context, db, HDB_F_DECRYPT, v5_prop, pd);
	if(ret)
	    krb5_warn(context, ret, "hdb_foreach");
	break;
    default:
	krb5_errx(context, 1, "unknown prop type: %d", type);
    }
    return ret;
}

static int
dump_database (krb5_context context, int type,
	       const char *database_name, HDB *db)
{
    krb5_error_code ret;
    struct prop_data pd;
    krb5_data data;

    pd.context      = context;
    pd.auth_context = NULL;
    pd.sock         = STDOUT_FILENO;
	
    ret = iterate (context, database_name, db, type, &pd);
    if (ret)
	krb5_errx(context, 1, "iterate failure");
    krb5_data_zero (&data);
    ret = krb5_write_message (context, &pd.sock, &data);
    if (ret)
	krb5_err(context, 1, ret, "krb5_write_message");

    return 0;
}

static int
propagate_database (krb5_context context, int type,
		    const char *database_name, 
		    HDB *db, krb5_ccache ccache,
		    int optidx, int argc, char **argv)
{
    krb5_principal server;
    krb5_error_code ret;
    int i, failed = 0;

    for(i = optidx; i < argc; i++){
	krb5_auth_context auth_context;
	int fd;
	struct prop_data pd;
	krb5_data data;

	char *port, portstr[NI_MAXSERV];
	char *host = argv[i];

	port = strchr(host, ':');
	if(port == NULL) {
	    snprintf(portstr, sizeof(portstr), "%u", 
		     ntohs(krb5_getportbyname (context, "hprop", "tcp", 
					       HPROP_PORT)));
	    port = portstr;
	} else
	    *port++ = '\0';

	fd = open_socket(context, host, port);
	if(fd < 0) {
	    failed++;
	    krb5_warn (context, errno, "connect %s", host);
	    continue;
	}

	ret = krb5_sname_to_principal(context, argv[i],
				      HPROP_NAME, KRB5_NT_SRV_HST, &server);
	if(ret) {
	    failed++;
	    krb5_warn(context, ret, "krb5_sname_to_principal(%s)", host);
	    close(fd);
	    continue;
	}

        if (local_realm) {
            krb5_realm my_realm;
            krb5_get_default_realm(context,&my_realm);

	    free (*krb5_princ_realm(context, server));
            krb5_princ_set_realm(context,server,&my_realm);
        }
    
	auth_context = NULL;
	ret = krb5_sendauth(context,
			    &auth_context,
			    &fd,
			    HPROP_VERSION,
			    NULL,
			    server,
			    AP_OPTS_MUTUAL_REQUIRED | AP_OPTS_USE_SUBKEY,
			    NULL, /* in_data */
			    NULL, /* in_creds */
			    ccache,
			    NULL,
			    NULL,
			    NULL);

	krb5_free_principal(context, server);

	if(ret) {
	    failed++;
	    krb5_warn(context, ret, "krb5_sendauth (%s)", host);
	    close(fd);
	    goto next_host;
	}
	
	pd.context      = context;
	pd.auth_context = auth_context;
	pd.sock         = fd;

	ret = iterate (context, database_name, db, type, &pd);
	if (ret) {
	    krb5_warnx(context, "iterate to host %s failed", host);
	    failed++;
	    goto next_host;
	}

	krb5_data_zero (&data);
	ret = krb5_write_priv_message(context, auth_context, &fd, &data);
	if(ret) {
	    krb5_warn(context, ret, "krb5_write_priv_message");
	    failed++;
	    goto next_host;
	}

	ret = krb5_read_priv_message(context, auth_context, &fd, &data);
	if(ret) {
	    krb5_warn(context, ret, "krb5_read_priv_message: %s", host);
	    failed++;
	    goto next_host;
	} else
	    krb5_data_free (&data);
	
    next_host:
	krb5_auth_con_free(context, auth_context);
	close(fd);
    }
    if (failed)
	return 1;
    return 0;
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache ccache = NULL;
    HDB *db = NULL;
    int optidx = 0;

    int type, exit_code;

    setprogname(argv[0]);

    if(getarg(args, num_args, argc, argv, &optidx))
	usage(1);

    if(help_flag)
	usage(0);
    
    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = krb5_init_context(&context);
    if(ret)
	exit(1);

    if(local_realm)
	krb5_set_default_realm(context, local_realm);

    if(v4_realm == NULL) {
	ret = krb5_get_default_realm(context, &v4_realm);
	if(ret)
	    krb5_err(context, 1, ret, "krb5_get_default_realm");
    }

    if(afs_cell == NULL) {
	afs_cell = strdup(v4_realm);
	if(afs_cell == NULL)
	    krb5_errx(context, 1, "out of memory");
	strlwr(afs_cell);
    }


    if(encrypt_flag && decrypt_flag)
	krb5_errx(context, 1, 
		  "only one of `--encrypt' and `--decrypt' is meaningful");

    if(source_type != NULL) {
	type = parse_source_type(source_type);
	if(type == 0)
	    krb5_errx(context, 1, "unknown source type `%s'", source_type);
    } else
	type = HPROP_HEIMDAL;

    if(!to_stdout)
	get_creds(context, &ccache);
    
    if(decrypt_flag || encrypt_flag) {
	ret = hdb_read_master_key(context, mkeyfile, &mkey5);
	if(ret && ret != ENOENT)
	    krb5_err(context, 1, ret, "hdb_read_master_key");
	if(ret)
	    krb5_errx(context, 1, "No master key file found");
    }
    
    if (IS_TYPE_V4(type) && v4_realm == NULL)
	krb5_errx(context, 1, "Its a Kerberos 4 database "
		  "but no realm configured");

    switch(type) {
    case HPROP_KASERVER:
	if (database == NULL)
	    database = DEFAULT_DATABASE;
	ka_use_null_salt = krb5_config_get_bool_default(context, NULL, FALSE, 
							"hprop", 
							"afs_uses_null_salt", 
							NULL);

	break;
    case HPROP_KRB4_DUMP:
	if (database == NULL)
	    krb5_errx(context, 1, "no dump file specified");
	
	break;
    case HPROP_MIT_DUMP:
	if (database == NULL)
	    krb5_errx(context, 1, "no dump file specified");
	break;
    case HPROP_HEIMDAL:
	ret = hdb_create (context, &db, database);
	if(ret)
	    krb5_err(context, 1, ret, "hdb_create: %s", database);
	ret = db->hdb_open(context, db, O_RDONLY, 0);
	if(ret)
	    krb5_err(context, 1, ret, "db->hdb_open");
	break;
    default:
	krb5_errx(context, 1, "unknown dump type `%d'", type);
	break;
    }

    if (to_stdout)
	exit_code = dump_database (context, type, database, db);
    else
	exit_code = propagate_database (context, type, database, 
					db, ccache, optidx, argc, argv);

    if(ccache != NULL)
	krb5_cc_destroy(context, ccache);
	
    if(db != NULL)
	(*db->hdb_destroy)(context, db);

    krb5_free_context(context);
    return exit_code;
}
