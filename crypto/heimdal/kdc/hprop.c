/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
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

RCSID("$Id: hprop.c,v 1.69 2002/04/18 10:18:35 joda Exp $");

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
v5_prop(krb5_context context, HDB *db, hdb_entry *entry, void *appdata)
{
    krb5_error_code ret;
    struct prop_data *pd = appdata;
    krb5_data data;

    if(encrypt_flag) {
	ret = hdb_seal_keys_mkey(context, entry, mkey5);
	if (ret) {
	    krb5_warn(context, ret, "hdb_seal_keys_mkey");
	    return ret;
	}
    }
    if(decrypt_flag) {
	ret = hdb_unseal_keys_mkey(context, entry, mkey5);
	if (ret) {
	    krb5_warn(context, ret, "hdb_unseal_keys_mkey");
	    return ret;
	}
    }	

    ret = hdb_entry2value(context, entry, &data);
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

#ifdef KRB4

static char realm_buf[REALM_SZ];

static int
kdb_prop(void *arg, Principal *p)
{
    int ret;
    struct v4_principal pr;

    memset(&pr, 0, sizeof(pr));

    if(p->attributes != 0) {
	warnx("%s.%s has non-zero attributes - skipping", 
	      p->name, p->instance);
	    return 0;
    }
    strlcpy(pr.name, p->name, sizeof(pr.name));
    strlcpy(pr.instance, p->instance, sizeof(pr.instance));

    copy_to_key(&p->key_low, &p->key_high, pr.key);
    pr.exp_date = p->exp_date;
    pr.mod_date = p->mod_date;
    strlcpy(pr.mod_name, p->mod_name, sizeof(pr.mod_name));
    strlcpy(pr.mod_instance, p->mod_instance, sizeof(pr.mod_instance));
    pr.max_life = p->max_life;
    pr.mkvno = p->kdc_key_ver;
    pr.kvno = p->key_version;
    
    ret = v4_prop(arg, &pr);
    memset(&pr, 0, sizeof(pr));
    return ret;
}

#endif /* KRB4 */

#ifndef KRB4
static time_t
krb_life_to_time(time_t start, int life)
{
    static int lifetimes[] = {
	  38400,   41055,   43894,   46929,   50174,   53643,   57352,   61318,
	  65558,   70091,   74937,   80119,   85658,   91581,   97914,  104684,
	 111922,  119661,  127935,  136781,  146239,  156350,  167161,  178720,
	 191077,  204289,  218415,  233517,  249664,  266926,  285383,  305116,
	 326213,  348769,  372885,  398668,  426234,  455705,  487215,  520904,
	 556921,  595430,  636601,  680618,  727680,  777995,  831789,  889303,
	 950794, 1016537, 1086825, 1161973, 1242318, 1328218, 1420057, 1518247,
	1623226, 1735464, 1855462, 1983758, 2120925, 2267576, 2424367, 2592000
    };

#if 0
    int i;
    double q = exp((log(2592000.0) - log(38400.0)) / 63);
    double x = 38400;
    for(i = 0; i < 64; i++) {
	lifetimes[i] = (int)x;
	x *= q;
    }
#endif

    if(life == 0xff)
	return NEVERDATE;
    if(life < 0x80)
	return start + life * 5 * 60;
    if(life > 0xbf)
	life = 0xbf;
    return start + lifetimes[life - 0x80];
}
#endif /* !KRB4 */

int
v4_prop(void *arg, struct v4_principal *p)
{
    struct prop_data *pd = arg;
    hdb_entry ent;
    krb5_error_code ret;

    memset(&ent, 0, sizeof(ent));

    ret = krb5_425_conv_principal(pd->context, p->name, p->instance, v4_realm,
				  &ent.principal);
    if(ret) {
	krb5_warn(pd->context, ret,
		  "krb5_425_conv_principal %s.%s@%s",
		  p->name, p->instance, v4_realm);
	return 0;
    }

    if(verbose_flag) {
	char *s;
	krb5_unparse_name_short(pd->context, ent.principal, &s);
	krb5_warnx(pd->context, "%s.%s -> %s", p->name, p->instance, s);
	free(s);
    }

    ent.kvno = p->kvno;
    ent.keys.len = 3;
    ent.keys.val = malloc(ent.keys.len * sizeof(*ent.keys.val));
    if(p->mkvno != -1) {
	ent.keys.val[0].mkvno = malloc (sizeof(*ent.keys.val[0].mkvno));
	*(ent.keys.val[0].mkvno) = p->mkvno;
    } else
	ent.keys.val[0].mkvno = NULL;
    ent.keys.val[0].salt = calloc(1, sizeof(*ent.keys.val[0].salt));
    ent.keys.val[0].salt->type = KRB5_PADATA_PW_SALT;
    ent.keys.val[0].key.keytype = ETYPE_DES_CBC_MD5;
    krb5_data_alloc(&ent.keys.val[0].key.keyvalue, sizeof(des_cblock));
    memcpy(ent.keys.val[0].key.keyvalue.data, p->key, 8);

    copy_Key(&ent.keys.val[0], &ent.keys.val[1]);
    ent.keys.val[1].key.keytype = ETYPE_DES_CBC_MD4;
    copy_Key(&ent.keys.val[0], &ent.keys.val[2]);
    ent.keys.val[2].key.keytype = ETYPE_DES_CBC_CRC;

    {
	int life = krb_life_to_time(0, p->max_life);
	if(life == NEVERDATE){
	    ent.max_life = NULL;
	} else {
	    /* clean up lifetime a bit */
	    if(life > 86400)
		life = (life + 86399) / 86400 * 86400;
	    else if(life > 3600)
		life = (life + 3599) / 3600 * 3600;
	    ALLOC(ent.max_life);
	    *ent.max_life = life;
	}
    }

    ALLOC(ent.valid_end);
    *ent.valid_end = p->exp_date;

    ret = krb5_make_principal(pd->context, &ent.created_by.principal,
			      v4_realm,
			      "kadmin",
			      "hprop",
			      NULL);
    if(ret){
	krb5_warn(pd->context, ret, "krb5_make_principal");
	ret = 0;
	goto out;
    }
    ent.created_by.time = time(NULL);
    ALLOC(ent.modified_by);
    ret = krb5_425_conv_principal(pd->context, p->mod_name, p->mod_instance, 
				  v4_realm, &ent.modified_by->principal);
    if(ret){
	krb5_warn(pd->context, ret, "%s.%s@%s", p->name, p->instance, v4_realm);
	ent.modified_by->principal = NULL;
	ret = 0;
	goto out;
    }
    ent.modified_by->time = p->mod_date;

    ent.flags.forwardable = 1;
    ent.flags.renewable = 1;
    ent.flags.proxiable = 1;
    ent.flags.postdate = 1;
    ent.flags.client = 1;
    ent.flags.server = 1;
    
    /* special case password changing service */
    if(strcmp(p->name, "changepw") == 0 && 
       strcmp(p->instance, "kerberos") == 0) {
	ent.flags.forwardable = 0;
	ent.flags.renewable = 0;
	ent.flags.proxiable = 0;
	ent.flags.postdate = 0;
	ent.flags.initial = 1;
	ent.flags.change_pw = 1;
    }

    ret = v5_prop(pd->context, NULL, &ent, pd);

    if (strcmp (p->name, "krbtgt") == 0
	&& strcmp (v4_realm, p->instance) != 0) {
	krb5_free_principal (pd->context, ent.principal);
	ret = krb5_425_conv_principal (pd->context, p->name,
				       v4_realm, p->instance,
				       &ent.principal);
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
    hdb_entry hdb;

    if(!kaspecials_flag
       && (flags & KAFNORMAL) == 0) /* remove special entries */
	return 0;
    memset(&hdb, 0, sizeof(hdb));
    ret = krb5_425_conv_principal(pd->context, ent->name, ent->instance, 
				  v4_realm, &hdb.principal);
    if(ret) {
	krb5_warn(pd->context, ret,
		  "krb5_425_conv_principal (%s.%s@%s)",
		  ent->name, ent->instance, v4_realm);
	return 0;
    }
    hdb.kvno = ntohl(ent->kvno);
    hdb.keys.len = 3;
    hdb.keys.val = malloc(hdb.keys.len * sizeof(*hdb.keys.val));
    hdb.keys.val[0].mkvno = NULL;
    hdb.keys.val[0].salt = calloc(1, sizeof(*hdb.keys.val[0].salt));
    if (ka_use_null_salt) {
	hdb.keys.val[0].salt->type = hdb_pw_salt;
	hdb.keys.val[0].salt->salt.data = NULL;
	hdb.keys.val[0].salt->salt.length = 0;
    } else {
	hdb.keys.val[0].salt->type = hdb_afs3_salt;
	hdb.keys.val[0].salt->salt.data = strdup(afs_cell);
	hdb.keys.val[0].salt->salt.length = strlen(afs_cell);
    }
    
    hdb.keys.val[0].key.keytype = ETYPE_DES_CBC_MD5;
    krb5_data_copy(&hdb.keys.val[0].key.keyvalue, ent->key, sizeof(ent->key));
    copy_Key(&hdb.keys.val[0], &hdb.keys.val[1]);
    hdb.keys.val[1].key.keytype = ETYPE_DES_CBC_MD4;
    copy_Key(&hdb.keys.val[0], &hdb.keys.val[2]);
    hdb.keys.val[2].key.keytype = ETYPE_DES_CBC_CRC;

    ALLOC(hdb.max_life);
    *hdb.max_life = ntohl(ent->max_life);

    if(ntohl(ent->valid_end) != NEVERDATE && ntohl(ent->valid_end) != -1){
	ALLOC(hdb.valid_end);
	*hdb.valid_end = ntohl(ent->valid_end);
    }
    
    if (ntohl(ent->pw_change) != NEVERDATE && 
	ent->pw_expire != 255 &&
	ent->pw_expire != 0) {
	ALLOC(hdb.pw_end);
	*hdb.pw_end = ntohl(ent->pw_change)
	    + 24 * 60 * 60 * ent->pw_expire;
    }

    ret = krb5_make_principal(pd->context, &hdb.created_by.principal,
			      v4_realm,
			      "kadmin",
			      "hprop",
			      NULL);
    hdb.created_by.time = time(NULL);

    if(ent->mod_ptr){
	struct ka_entry mod;
	ALLOC(hdb.modified_by);
	read_block(pd->context, fd, ntohl(ent->mod_ptr), &mod, sizeof(mod));
	
	krb5_425_conv_principal(pd->context, mod.name, mod.instance, v4_realm, 
				&hdb.modified_by->principal);
	hdb.modified_by->time = ntohl(ent->mod_time);
	memset(&mod, 0, sizeof(mod));
    }

    hdb.flags.forwardable = 1;
    hdb.flags.renewable = 1;
    hdb.flags.proxiable = 1;
    hdb.flags.postdate = 1;
    /* XXX - AFS 3.4a creates krbtgt.REALMOFCELL as NOTGS+NOSEAL */
    if (strcmp(ent->name, "krbtgt") == 0 &&
	(flags & (KAFNOTGS|KAFNOSEAL)) == (KAFNOTGS|KAFNOSEAL))
	flags &= ~(KAFNOTGS|KAFNOSEAL);

    hdb.flags.client = (flags & KAFNOTGS) == 0;
    hdb.flags.server = (flags & KAFNOSEAL) == 0;

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
#ifdef KRB4
      "|krb4-db"
#endif
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
    krb5_get_init_creds_opt init_opts;
    krb5_preauthtype preauth = KRB5_PADATA_ENC_TIMESTAMP;
    krb5_creds creds;
    
    ret = krb5_kt_register(context, &hdb_kt_ops);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_register");

    ret = krb5_kt_resolve(context, ktname, &keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_resolve");
    
    ret = krb5_make_principal(context, &client, NULL, 
			      "kadmin", HPROP_NAME, NULL);
    if(ret) krb5_err(context, 1, ret, "krb5_make_principal");

    krb5_get_init_creds_opt_init(&init_opts);
    krb5_get_init_creds_opt_set_preauth_list(&init_opts, &preauth, 1);

    ret = krb5_get_init_creds_keytab(context, &creds, client, keytab, 0, NULL, &init_opts);
    if(ret) krb5_err(context, 1, ret, "krb5_get_init_creds");
    
    ret = krb5_kt_close(context, keytab);
    if(ret) krb5_err(context, 1, ret, "krb5_kt_close");
    
    ret = krb5_cc_gen_new(context, &krb5_mcc_ops, cache);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_gen_new");

    ret = krb5_cc_initialize(context, *cache, client);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_initialize");

    krb5_free_principal(context, client);

    ret = krb5_cc_store_cred(context, *cache, &creds);
    if(ret) krb5_err(context, 1, ret, "krb5_cc_store_cred");

    krb5_free_creds_contents(context, &creds);
}

enum hprop_source {
    HPROP_HEIMDAL = 1,
    HPROP_KRB4_DB,
    HPROP_KRB4_DUMP,
    HPROP_KASERVER,
    HPROP_MIT_DUMP
};

#define IS_TYPE_V4(X) ((X) == HPROP_KRB4_DB || (X) == HPROP_KRB4_DUMP || (X) == HPROP_KASERVER)

struct {
    int type;
    const char *name;
} types[] = {
    { HPROP_HEIMDAL,	"heimdal" },
    { HPROP_KRB4_DUMP,	"krb4-dump" },
#ifdef KRB4
    { HPROP_KRB4_DB,	"krb4-db" },
#endif
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

static void
iterate (krb5_context context,
	 const char *database,
	 HDB *db,
	 int type,
	 struct prop_data *pd)
{
    int ret;

    switch(type) {
    case HPROP_KRB4_DUMP:
	ret = v4_prop_dump(pd, database);
	break;
#ifdef KRB4
    case HPROP_KRB4_DB:
	ret = kerb_db_iterate ((k_iter_proc_t)kdb_prop, pd);
	if(ret)
	    krb5_errx(context, 1, "kerb_db_iterate: %s", 
		      krb_get_err_text(ret));
	break;
#endif /* KRB4 */
    case HPROP_KASERVER:
	ret = ka_dump(pd, database);
	if(ret)
	    krb5_err(context, 1, ret, "ka_dump");
	break;
    case HPROP_MIT_DUMP:
	ret = mit_prop_dump(pd, database);
	if (ret)
	    krb5_errx(context, 1, "mit_prop_dump: %s",
		      krb5_get_err_text(context, ret));
	break;
    case HPROP_HEIMDAL:
	ret = hdb_foreach(context, db, HDB_F_DECRYPT, v5_prop, pd);
	if(ret)
	    krb5_err(context, 1, ret, "hdb_foreach");
	break;
    }
}

static int
dump_database (krb5_context context, int type,
	       const char *database, HDB *db)
{
    krb5_error_code ret;
    struct prop_data pd;
    krb5_data data;

    pd.context      = context;
    pd.auth_context = NULL;
    pd.sock         = STDOUT_FILENO;
	
    iterate (context, database, db, type, &pd);
    krb5_data_zero (&data);
    ret = krb5_write_message (context, &pd.sock, &data);
    if (ret)
	krb5_err(context, 1, ret, "krb5_write_message");

    return 0;
}

static int
propagate_database (krb5_context context, int type,
		    const char *database, 
		    HDB *db, krb5_ccache ccache,
		    int optind, int argc, char **argv)
{
    krb5_principal server;
    krb5_error_code ret;
    int i;

    for(i = optind; i < argc; i++){
	krb5_auth_context auth_context;
	int fd;
	struct prop_data pd;
	krb5_data data;

	char *port, portstr[NI_MAXSERV];
	
	port = strchr(argv[i], ':');
	if(port == NULL) {
	    snprintf(portstr, sizeof(portstr), "%u", 
		     ntohs(krb5_getportbyname (context, "hprop", "tcp", 
					       HPROP_PORT)));
	    port = portstr;
	} else
	    *port++ = '\0';

	fd = open_socket(context, argv[i], port);
	if(fd < 0) {
	    krb5_warn (context, errno, "connect %s", argv[i]);
	    continue;
	}

	ret = krb5_sname_to_principal(context, argv[i],
				      HPROP_NAME, KRB5_NT_SRV_HST, &server);
	if(ret) {
	    krb5_warn(context, ret, "krb5_sname_to_principal(%s)", argv[i]);
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
			    AP_OPTS_MUTUAL_REQUIRED,
			    NULL, /* in_data */
			    NULL, /* in_creds */
			    ccache,
			    NULL,
			    NULL,
			    NULL);

	krb5_free_principal(context, server);

	if(ret) {
	    krb5_warn(context, ret, "krb5_sendauth");
	    close(fd);
	    continue;
	}
	
	pd.context      = context;
	pd.auth_context = auth_context;
	pd.sock         = fd;

	iterate (context, database, db, type, &pd);

	krb5_data_zero (&data);
	ret = krb5_write_priv_message(context, auth_context, &fd, &data);
	if(ret)
	    krb5_warn(context, ret, "krb5_write_priv_message");

	ret = krb5_read_priv_message(context, auth_context, &fd, &data);
	if(ret)
	    krb5_warn(context, ret, "krb5_read_priv_message");
	else
	    krb5_data_free (&data);
	
	krb5_auth_con_free(context, auth_context);
	close(fd);
    }
    return 0;
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_ccache ccache = NULL;
    HDB *db = NULL;
    int optind = 0;

    int type = 0;

    setprogname(argv[0]);

    if(getarg(args, num_args, argc, argv, &optind))
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
	if(type != 0)
	    krb5_errx(context, 1, "more than one database type specified");
	type = parse_source_type(source_type);
	if(type == 0)
	    krb5_errx(context, 1, "unknown source type `%s'", source_type);
    } else if(type == 0)
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
    
#ifdef KRB4
    if (IS_TYPE_V4(type)) {
	int e;

	if (v4_realm == NULL) {
	    e = krb_get_lrealm(realm_buf, 1);
	    if(e)
		krb5_errx(context, 1, "krb_get_lrealm: %s",
			  krb_get_err_text(e));
	    v4_realm = realm_buf;
	}
    }
#endif

    switch(type) {
#ifdef KRB4
    case HPROP_KRB4_DB:
	if (database == NULL)
	    krb5_errx(context, 1, "no database specified");
	break;
#endif
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
	ret = db->open(context, db, O_RDONLY, 0);
	if(ret)
	    krb5_err(context, 1, ret, "db->open");
	break;
    default:
	krb5_errx(context, 1, "unknown dump type `%d'", type);
	break;
    }

    if (to_stdout)
	dump_database (context, type, database, db);
    else
	propagate_database (context, type, database, 
			    db, ccache, optind, argc, argv);

    if(ccache != NULL)
	krb5_cc_destroy(context, ccache);
	
    if(db != NULL)
	(*db->destroy)(context, db);

    krb5_free_context(context);
    return 0;
}
