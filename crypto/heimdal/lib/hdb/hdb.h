/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

/* $Id: hdb.h,v 1.31 2000/07/08 16:03:37 joda Exp $ */

#ifndef __HDB_H__
#define __HDB_H__

#include <hdb_err.h>

#include <hdb_asn1.h>

enum hdb_lockop{ HDB_RLOCK, HDB_WLOCK };

/* flags for various functions */
#define HDB_F_DECRYPT	1 /* decrypt keys */
#define HDB_F_REPLACE	2 /* replace entry */

/* key usage for master key */
#define HDB_KU_MKEY	0x484442

typedef struct hdb_master_key_data *hdb_master_key;

typedef struct HDB{
    void *db;
    void *dbc;
    char *name;
    int master_key_set;
    hdb_master_key master_key;
    int openp;

    krb5_error_code (*open)(krb5_context, struct HDB*, int, mode_t);
    krb5_error_code (*close)(krb5_context, struct HDB*);
    krb5_error_code (*fetch)(krb5_context, struct HDB*, unsigned, hdb_entry*);
    krb5_error_code (*store)(krb5_context, struct HDB*, unsigned, hdb_entry*);
    krb5_error_code (*remove)(krb5_context, struct HDB*, hdb_entry*);
    krb5_error_code (*firstkey)(krb5_context, struct HDB*, 
				unsigned, hdb_entry*);
    krb5_error_code (*nextkey)(krb5_context, struct HDB*, 
			       unsigned, hdb_entry*);
    krb5_error_code (*lock)(krb5_context, struct HDB*, int operation);
    krb5_error_code (*unlock)(krb5_context, struct HDB*);
    krb5_error_code (*rename)(krb5_context, struct HDB*, const char*);
    krb5_error_code (*_get)(krb5_context, struct HDB*, krb5_data, krb5_data*);
    krb5_error_code (*_put)(krb5_context, struct HDB*, int, 
			    krb5_data, krb5_data);
    krb5_error_code (*_del)(krb5_context, struct HDB*, krb5_data);
    krb5_error_code (*destroy)(krb5_context, struct HDB*);
}HDB;

#define HDB_DB_DIR "/var/heimdal"
#define HDB_DEFAULT_DB HDB_DB_DIR "/heimdal"
#define HDB_DB_FORMAT_ENTRY "hdb/db-format"

typedef krb5_error_code (*hdb_foreach_func_t)(krb5_context, HDB*,
					      hdb_entry*, void*);
extern krb5_kt_ops hdb_kt_ops;

#include <hdb-protos.h>

#endif /* __HDB_H__ */
