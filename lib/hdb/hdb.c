/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

#include "hdb_locl.h"

RCSID("$Id: hdb.c 20214 2007-02-09 21:51:10Z lha $");

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

struct hdb_method {
    const char *prefix;
    krb5_error_code (*create)(krb5_context, HDB **, const char *filename);
};

static struct hdb_method methods[] = {
#if HAVE_DB1 || HAVE_DB3
    {"db:",	hdb_db_create},
#endif
#if HAVE_NDBM
    {"ndbm:",	hdb_ndbm_create},
#endif
#if defined(OPENLDAP) && !defined(OPENLDAP_MODULE)
    {"ldap:",	hdb_ldap_create},
    {"ldapi:",	hdb_ldapi_create},
#endif
#ifdef HAVE_LDB /* Used for integrated samba build */
    {"ldb:",	hdb_ldb_create},
#endif
    {NULL,	NULL}
};

#if HAVE_DB1 || HAVE_DB3
static struct hdb_method dbmetod = {"",	hdb_db_create };
#elif defined(HAVE_NDBM)
static struct hdb_method dbmetod = {"",	hdb_ndbm_create };
#endif


krb5_error_code
hdb_next_enctype2key(krb5_context context,
		     const hdb_entry *e,
		     krb5_enctype enctype,
		     Key **key)
{
    Key *k;
    
    for (k = *key ? (*key) + 1 : e->keys.val;
	 k < e->keys.val + e->keys.len; 
	 k++) 
    {
	if(k->key.keytype == enctype){
	    *key = k;
	    return 0;
	}
    }
    krb5_set_error_string(context, "No next enctype %d for hdb-entry", 
			  (int)enctype);
    return KRB5_PROG_ETYPE_NOSUPP; /* XXX */
}

krb5_error_code
hdb_enctype2key(krb5_context context, 
		hdb_entry *e, 
		krb5_enctype enctype, 
		Key **key)
{
    *key = NULL;
    return hdb_next_enctype2key(context, e, enctype, key);
}

void
hdb_free_key(Key *key)
{
    memset(key->key.keyvalue.data, 
	   0,
	   key->key.keyvalue.length);
    free_Key(key);
    free(key);
}


krb5_error_code
hdb_lock(int fd, int operation)
{
    int i, code = 0;

    for(i = 0; i < 3; i++){
	code = flock(fd, (operation == HDB_RLOCK ? LOCK_SH : LOCK_EX) | LOCK_NB);
	if(code == 0 || errno != EWOULDBLOCK)
	    break;
	sleep(1);
    }
    if(code == 0)
	return 0;
    if(errno == EWOULDBLOCK)
	return HDB_ERR_DB_INUSE;
    return HDB_ERR_CANT_LOCK_DB;
}

krb5_error_code
hdb_unlock(int fd)
{
    int code;
    code = flock(fd, LOCK_UN);
    if(code)
	return 4711 /* XXX */;
    return 0;
}

void
hdb_free_entry(krb5_context context, hdb_entry_ex *ent)
{
    int i;

    if (ent->free_entry)
	(*ent->free_entry)(context, ent);

    for(i = 0; i < ent->entry.keys.len; ++i) {
	Key *k = &ent->entry.keys.val[i];

	memset (k->key.keyvalue.data, 0, k->key.keyvalue.length);
    }
    free_hdb_entry(&ent->entry);
}

krb5_error_code
hdb_foreach(krb5_context context,
	    HDB *db,
	    unsigned flags,
	    hdb_foreach_func_t func,
	    void *data)
{
    krb5_error_code ret;
    hdb_entry_ex entry;
    ret = db->hdb_firstkey(context, db, flags, &entry);
    if (ret == 0)
	krb5_clear_error_string(context);
    while(ret == 0){
	ret = (*func)(context, db, &entry, data);
	hdb_free_entry(context, &entry);
	if(ret == 0)
	    ret = db->hdb_nextkey(context, db, flags, &entry);
    }
    if(ret == HDB_ERR_NOENTRY)
	ret = 0;
    return ret;
}

krb5_error_code
hdb_check_db_format(krb5_context context, HDB *db)
{
    krb5_data tag;
    krb5_data version;
    krb5_error_code ret, ret2;
    unsigned ver;
    int foo;

    ret = db->hdb_lock(context, db, HDB_RLOCK);
    if (ret)
	return ret;

    tag.data = HDB_DB_FORMAT_ENTRY;
    tag.length = strlen(tag.data);
    ret = (*db->hdb__get)(context, db, tag, &version);
    ret2 = db->hdb_unlock(context, db);
    if(ret)
	return ret;
    if (ret2)
	return ret2;
    foo = sscanf(version.data, "%u", &ver);
    krb5_data_free (&version);
    if (foo != 1)
	return HDB_ERR_BADVERSION;
    if(ver != HDB_DB_FORMAT)
	return HDB_ERR_BADVERSION;
    return 0;
}

krb5_error_code
hdb_init_db(krb5_context context, HDB *db)
{
    krb5_error_code ret, ret2;
    krb5_data tag;
    krb5_data version;
    char ver[32];
    
    ret = hdb_check_db_format(context, db);
    if(ret != HDB_ERR_NOENTRY)
	return ret;
    
    ret = db->hdb_lock(context, db, HDB_WLOCK);
    if (ret)
	return ret;

    tag.data = HDB_DB_FORMAT_ENTRY;
    tag.length = strlen(tag.data);
    snprintf(ver, sizeof(ver), "%u", HDB_DB_FORMAT);
    version.data = ver;
    version.length = strlen(version.data) + 1; /* zero terminated */
    ret = (*db->hdb__put)(context, db, 0, tag, version);
    ret2 = db->hdb_unlock(context, db);
    if (ret) {
	if (ret2)
	    krb5_clear_error_string(context);
	return ret;
    }
    return ret2;
}

#ifdef HAVE_DLOPEN

 /*
 * Load a dynamic backend from /usr/heimdal/lib/hdb_NAME.so,
 * looking for the hdb_NAME_create symbol.
 */

static const struct hdb_method *
find_dynamic_method (krb5_context context,
		     const char *filename, 
		     const char **rest)
{
    static struct hdb_method method;
    struct hdb_so_method *mso;
    char *prefix, *path, *symbol;
    const char *p;
    void *dl;
    size_t len;
    
    p = strchr(filename, ':');

    /* if no prefix, don't know what module to load, just ignore it */
    if (p == NULL)
	return NULL;

    len = p - filename;
    *rest = filename + len + 1;
    
    prefix = strndup(filename, len);
    if (prefix == NULL)
	krb5_errx(context, 1, "out of memory");
    
    if (asprintf(&path, LIBDIR "/hdb_%s.so", prefix) == -1)
	krb5_errx(context, 1, "out of memory");

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif

    dl = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (dl == NULL) {
	krb5_warnx(context, "error trying to load dynamic module %s: %s\n",
		   path, dlerror());
	free(prefix);
	free(path);
	return NULL;
    }
    
    if (asprintf(&symbol, "hdb_%s_interface", prefix) == -1)
	krb5_errx(context, 1, "out of memory");
	
    mso = dlsym(dl, symbol);
    if (mso == NULL) {
	krb5_warnx(context, "error finding symbol %s in %s: %s\n", 
		   symbol, path, dlerror());
	dlclose(dl);
	free(symbol);
	free(prefix);
	free(path);
	return NULL;
    }
    free(path);
    free(symbol);

    if (mso->version != HDB_INTERFACE_VERSION) {
	krb5_warnx(context, 
		   "error wrong version in shared module %s "
		   "version: %d should have been %d\n", 
		   prefix, mso->version, HDB_INTERFACE_VERSION);
	dlclose(dl);
	free(prefix);
	return NULL;
    }

    if (mso->create == NULL) {
	krb5_errx(context, 1,
		  "no entry point function in shared mod %s ",
		   prefix);
	dlclose(dl);
	free(prefix);
	return NULL;
    }

    method.create = mso->create;
    method.prefix = prefix;

    return &method;
}
#endif /* HAVE_DLOPEN */

/*
 * find the relevant method for `filename', returning a pointer to the
 * rest in `rest'.
 * return NULL if there's no such method.
 */

static const struct hdb_method *
find_method (const char *filename, const char **rest)
{
    const struct hdb_method *h;

    for (h = methods; h->prefix != NULL; ++h) {
	if (strncmp (filename, h->prefix, strlen(h->prefix)) == 0) {
	    *rest = filename + strlen(h->prefix);
	    return h;
	}
    }
#if defined(HAVE_DB1) || defined(HAVE_DB3) || defined(HAVE_NDBM)
    if (strncmp(filename, "/", 1) == 0
	|| strncmp(filename, "./", 2) == 0
	|| strncmp(filename, "../", 3) == 0)
    {
	*rest = filename;
	return &dbmetod;
    }
#endif

    return NULL;
}

krb5_error_code
hdb_list_builtin(krb5_context context, char **list)
{
    const struct hdb_method *h;
    size_t len = 0;
    char *buf = NULL;

    for (h = methods; h->prefix != NULL; ++h) {
	if (h->prefix[0] == '\0')
	    continue;
	len += strlen(h->prefix) + 2;
    }

    len += 1;
    buf = malloc(len);
    if (buf == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }
    buf[0] = '\0';

    for (h = methods; h->prefix != NULL; ++h) {
	if (h != methods)
	    strlcat(buf, ", ", len);
	strlcat(buf, h->prefix, len);
    }
    *list = buf;
    return 0;
}

krb5_error_code
hdb_create(krb5_context context, HDB **db, const char *filename)
{
    const struct hdb_method *h;
    const char *residual;

    if(filename == NULL)
	filename = HDB_DEFAULT_DB;
    krb5_add_et_list(context, initialize_hdb_error_table_r);
    h = find_method (filename, &residual);
#ifdef HAVE_DLOPEN
    if (h == NULL)
	h = find_dynamic_method (context, filename, &residual);
#endif
    if (h == NULL)
	krb5_errx(context, 1, "No database support for %s", filename);
    return (*h->create)(context, db, residual);
}
