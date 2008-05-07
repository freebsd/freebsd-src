/*
 * Copyright (c) 1997-2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 *
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

RCSID("$Id: default_config.c 21296 2007-06-25 14:49:11Z lha $");

krb5_error_code
krb5_kdc_set_dbinfo(krb5_context context, struct krb5_kdc_configuration *c)
{
    struct hdb_dbinfo *info, *d;
    krb5_error_code ret;
    int i;

    /* fetch the databases */
    ret = hdb_get_dbinfo(context, &info);
    if (ret)
	return ret;
    
    d = NULL;
    while ((d = hdb_dbinfo_get_next(info, d)) != NULL) {
	void *ptr;
	
	ptr = realloc(c->db, (c->num_db + 1) * sizeof(*c->db));
	if (ptr == NULL) {
	    ret = ENOMEM;
	    krb5_set_error_string(context, "out of memory");
	    goto out;
	}
	c->db = ptr;
	
	ret = hdb_create(context, &c->db[c->num_db], 
			 hdb_dbinfo_get_dbname(context, d));
	if(ret)
	    goto out;
	
	ret = hdb_set_master_keyfile(context, c->db[c->num_db], 
				     hdb_dbinfo_get_mkey_file(context, d));
	if (ret)
	    goto out;
	
	c->num_db++;

	kdc_log(context, c, 0, "label: %s",
		hdb_dbinfo_get_label(context, d));
	kdc_log(context, c, 0, "\tdbname: %s",
		hdb_dbinfo_get_dbname(context, d));
	kdc_log(context, c, 0, "\tmkey_file: %s",
		hdb_dbinfo_get_mkey_file(context, d));
	kdc_log(context, c, 0, "\tacl_file: %s",
		hdb_dbinfo_get_acl_file(context, d));
    }
    hdb_free_dbinfo(context, &info);

    return 0;
out:
    for (i = 0; i < c->num_db; i++)
	if (c->db[i] && c->db[i]->hdb_destroy)
	    (*c->db[i]->hdb_destroy)(context, c->db[i]);
    c->num_db = 0;
    free(c->db);
    c->db = NULL;
 
    hdb_free_dbinfo(context, &info);

    return ret;
}


