/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

RCSID("$Id: misc.c 21106 2007-06-18 10:18:11Z lha $");

struct timeval _kdc_now;

krb5_error_code
_kdc_db_fetch(krb5_context context,
	      krb5_kdc_configuration *config,
	      krb5_const_principal principal,
	      unsigned flags,
	      HDB **db,
	      hdb_entry_ex **h)
{
    hdb_entry_ex *ent;
    krb5_error_code ret;
    int i;

    ent = calloc (1, sizeof (*ent));
    if (ent == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }

    for(i = 0; i < config->num_db; i++) {
	ret = config->db[i]->hdb_open(context, config->db[i], O_RDONLY, 0);
	if (ret) {
	    kdc_log(context, config, 0, "Failed to open database: %s", 
		    krb5_get_err_text(context, ret));
	    continue;
	}
	ret = config->db[i]->hdb_fetch(context, 
				       config->db[i],
				       principal,
				       flags | HDB_F_DECRYPT,
				       ent);
	config->db[i]->hdb_close(context, config->db[i]);
	if(ret == 0) {
	    if (db)
		*db = config->db[i];
	    *h = ent;
	    return 0;
	}
    }
    free(ent);
    krb5_set_error_string(context, "no such entry found in hdb");
    return  HDB_ERR_NOENTRY;
}

void
_kdc_free_ent(krb5_context context, hdb_entry_ex *ent)
{
    hdb_free_entry (context, ent);
    free (ent);
}

/*
 * Use the order list of preferred encryption types and sort the
 * available keys and return the most preferred key.
 */

krb5_error_code
_kdc_get_preferred_key(krb5_context context,
		       krb5_kdc_configuration *config,
		       hdb_entry_ex *h,
		       const char *name,
		       krb5_enctype *enctype,
		       Key **key)
{
    const krb5_enctype *p;
    krb5_error_code ret;
    int i;

    p = krb5_kerberos_enctypes(context);

    for (i = 0; p[i] != ETYPE_NULL; i++) {
	if (krb5_enctype_valid(context, p[i]) != 0)
	    continue;
	ret = hdb_enctype2key(context, &h->entry, p[i], key);
	if (ret == 0) {
	    *enctype = p[i];
	    return 0;
	}
    }

    krb5_set_error_string(context, "No valid kerberos key found for %s", name);
    return EINVAL;
}

