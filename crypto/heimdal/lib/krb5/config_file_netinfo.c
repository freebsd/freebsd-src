/*
 * Copyright (c) 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
RCSID("$Id: config_file_netinfo.c,v 1.2 1999/12/02 17:05:08 joda Exp $");

/*
 * Netinfo implementation from Luke Howard <lukeh@xedoc.com.au>
 */

#ifdef HAVE_NETINFO
#include <netinfo/ni.h>
static ni_status
ni_proplist2binding(ni_proplist *pl, krb5_config_section **ret)
{
    int i, j;
    krb5_config_section **next = NULL;

    for (i = 0; i < pl->ni_proplist_len; i++) {
	if (!strcmp(pl->nipl_val[i].nip_name, "name"))
	    continue;

	for (j = 0; j < pl->nipl_val[i].nip_val.ni_namelist_len; j++) {
	    krb5_config_binding *b;

	    b = malloc(sizeof(*b));
	    if (b == NULL)
		return NI_FAILED;
	
	    b->next = NULL;
	    b->type = krb5_config_string;
	    b->name = ni_name_dup(pl->nipl_val[i].nip_name);
	    b->u.string = ni_name_dup(pl->nipl_val[i].nip_val.ninl_val[j]);

	    if (next == NULL) {
		*ret = b;
	    } else {
		*next = b;
	    }
	    next = &b->next;
	}
    }
    return NI_OK;
}

static ni_status
ni_idlist2binding(void *ni, ni_idlist *idlist, krb5_config_section **ret)
{
    int i;
    ni_status nis;
    krb5_config_section **next;

    for (i = 0; i < idlist->ni_idlist_len; i++) {
	ni_proplist pl;
        ni_id nid;
	ni_idlist children;
	krb5_config_binding *b;
	ni_index index;

	nid.nii_instance = 0;
	nid.nii_object = idlist->ni_idlist_val[i];

	nis = ni_read(ni, &nid, &pl);

	if (nis != NI_OK) {
	     return nis;
	}
	index = ni_proplist_match(pl, "name", NULL);
	b = malloc(sizeof(*b));
	if (b == NULL) return NI_FAILED;

	if (i == 0) {
	    *ret = b;
	} else {
	    *next = b;
	}

	b->type = krb5_config_list;
	b->name = ni_name_dup(pl.nipl_val[index].nip_val.ninl_val[0]);
	b->next = NULL;
	b->u.list = NULL;

	/* get the child directories */
	nis = ni_children(ni, &nid, &children);
	if (nis == NI_OK) {
	    nis = ni_idlist2binding(ni, &children, &b->u.list);
	    if (nis != NI_OK) {
		return nis;
	    }
	}

	nis = ni_proplist2binding(&pl, b->u.list == NULL ? &b->u.list : &b->u.list->next);
	ni_proplist_free(&pl);
	if (nis != NI_OK) {
	    return nis;
	}
	next = &b->next;
    }
    ni_idlist_free(idlist);
    return NI_OK;
}

krb5_error_code
krb5_config_parse_file (const char *fname, krb5_config_section **res)
{
    void *ni = NULL, *lastni = NULL;
    int i;
    ni_status nis;
    ni_id nid;
    ni_idlist children;

    krb5_config_section *s;
    int ret;

    s = NULL;

    for (i = 0; i < 256; i++) {
	if (i == 0) {
	    nis = ni_open(NULL, ".", &ni);
	} else {
	    if (lastni != NULL) ni_free(lastni);
	    lastni = ni;
	    nis = ni_open(lastni, "..", &ni);
	}
	if (nis != NI_OK)
	    break;
	nis = ni_pathsearch(ni, &nid, "/locations/kerberos");
	if (nis == NI_OK) {
	    nis = ni_children(ni, &nid, &children);
	    if (nis != NI_OK)
		break;
	    nis = ni_idlist2binding(ni, &children, &s);
	    break;
	}
    }

    if (ni != NULL) ni_free(ni);
    if (ni != lastni && lastni != NULL) ni_free(lastni);

    ret = (nis == NI_OK) ? 0 : -1;
    if (ret == 0) {
	*res = s;
    } else {
	*res = NULL;
    }
    return ret;
}
#endif /* HAVE_NETINFO */
