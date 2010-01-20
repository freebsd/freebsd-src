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

/* $Id: hdb.h 22198 2007-12-07 13:09:25Z lha $ */

#ifndef __HDB_H__
#define __HDB_H__

#include <hdb_err.h>

#include <heim_asn1.h>
#include <hdb_asn1.h>

struct hdb_dbinfo;

enum hdb_lockop{ HDB_RLOCK, HDB_WLOCK };

/* flags for various functions */
#define HDB_F_DECRYPT		1	/* decrypt keys */
#define HDB_F_REPLACE		2	/* replace entry */
#define HDB_F_GET_CLIENT	4	/* fetch client */
#define HDB_F_GET_SERVER	8	/* fetch server */
#define HDB_F_GET_KRBTGT	16	/* fetch krbtgt */
#define HDB_F_GET_ANY		28	/* fetch any of client,server,krbtgt */
#define HDB_F_CANON		32	/* want canonicalition */

/* key usage for master key */
#define HDB_KU_MKEY	0x484442

typedef struct hdb_master_key_data *hdb_master_key;

typedef struct hdb_entry_ex {
    void *ctx;
    hdb_entry entry;
    void (*free_entry)(krb5_context, struct hdb_entry_ex *);
} hdb_entry_ex;


typedef struct HDB{
    void *hdb_db;
    void *hdb_dbc;
    char *hdb_name;
    int hdb_master_key_set;
    hdb_master_key hdb_master_key;
    int hdb_openp;

    krb5_error_code (*hdb_open)(krb5_context,
				struct HDB*,
				int,
				mode_t);
    krb5_error_code (*hdb_close)(krb5_context, 
				 struct HDB*);
    void	    (*hdb_free)(krb5_context,
				struct HDB*,
				hdb_entry_ex*);
    krb5_error_code (*hdb_fetch)(krb5_context,
				 struct HDB*,
				 krb5_const_principal,
				 unsigned,
				 hdb_entry_ex*);
    krb5_error_code (*hdb_store)(krb5_context,
				 struct HDB*,
				 unsigned,
				 hdb_entry_ex*);
    krb5_error_code (*hdb_remove)(krb5_context,
				  struct HDB*,
				  krb5_const_principal);
    krb5_error_code (*hdb_firstkey)(krb5_context,
				    struct HDB*,
				    unsigned,
				    hdb_entry_ex*);
    krb5_error_code (*hdb_nextkey)(krb5_context,
				   struct HDB*,
				   unsigned,
				   hdb_entry_ex*);
    krb5_error_code (*hdb_lock)(krb5_context,
				struct HDB*,
				int operation);
    krb5_error_code (*hdb_unlock)(krb5_context,
				  struct HDB*);
    krb5_error_code (*hdb_rename)(krb5_context,
				  struct HDB*,
				  const char*);
    krb5_error_code (*hdb__get)(krb5_context,
				struct HDB*,
				krb5_data,
				krb5_data*);
    krb5_error_code (*hdb__put)(krb5_context,
				struct HDB*,
				int, 
				krb5_data,
				krb5_data);
    krb5_error_code (*hdb__del)(krb5_context, 
				struct HDB*,
				krb5_data);
    krb5_error_code (*hdb_destroy)(krb5_context,
				   struct HDB*);
}HDB;

#define HDB_INTERFACE_VERSION	4

struct hdb_so_method {
    int version;
    const char *prefix;
    krb5_error_code (*create)(krb5_context, HDB **, const char *filename);
};

typedef krb5_error_code (*hdb_foreach_func_t)(krb5_context, HDB*,
					      hdb_entry_ex*, void*);
extern krb5_kt_ops hdb_kt_ops;

#include <hdb-protos.h>

#endif /* __HDB_H__ */
