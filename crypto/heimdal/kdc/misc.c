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

RCSID("$Id: misc.c,v 1.22 2001/01/30 03:54:21 assar Exp $");

struct timeval now;

krb5_error_code
db_fetch(krb5_principal principal, hdb_entry **h)
{
    hdb_entry *ent;
    krb5_error_code ret = HDB_ERR_NOENTRY;
    int i;

    ent = malloc (sizeof (*ent));
    if (ent == NULL)
	return ENOMEM;
    ent->principal = principal;

    for(i = 0; i < num_db; i++) {
	ret = db[i]->open(context, db[i], O_RDONLY, 0);
	if (ret) {
	    kdc_log(0, "Failed to open database: %s", 
		    krb5_get_err_text(context, ret));
	    continue;
	}
	ret = db[i]->fetch(context, db[i], HDB_F_DECRYPT, ent);
	db[i]->close(context, db[i]);
	if(ret == 0) {
	    *h = ent;
	    return 0;
	}
    }
    free(ent);
    return ret;
}

void
free_ent(hdb_entry *ent)
{
    hdb_free_entry (context, ent);
    free (ent);
}

