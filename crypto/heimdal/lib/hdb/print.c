/*
 * Copyright (c) 1999-2001 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "hdb_locl.h"
#include <ctype.h>

RCSID("$Id: print.c,v 1.5 2001/01/26 15:08:36 joda Exp $");

/* 
   This is the present contents of a dump line. This might change at
   any time. Fields are separated by white space.

  principal
  keyblock
  	kvno
	keys...
		mkvno
		enctype
		keyvalue
		salt (- means use normal salt)
  creation date and principal
  modification date and principal
  principal valid from date (not used)
  principal valid end date (not used)
  principal key expires (not used)
  max ticket life
  max renewable life
  flags
  */

static void
append_hex(char *str, krb5_data *data)
{
    int i, s = 1;
    char *p;

    p = data->data;
    for(i = 0; i < data->length; i++)
	if(!isalnum((unsigned char)p[i]) && p[i] != '.'){
	    s = 0;
	    break;
	}
    if(s){
	p = calloc(1, data->length + 2 + 1);
	p[0] = '\"';
	p[data->length + 1] = '\"';
	memcpy(p + 1, data->data, data->length);
    }else{
	const char *xchars = "0123456789abcdef";
	char *q = p = malloc(data->length * 2 + 1);
	for(i = 0; i < data->length; i++) {
	    unsigned char c = ((u_char*)data->data)[i];
	    *q++ = xchars[(c & 0xf0) >> 4];
	    *q++ = xchars[(c & 0xf)];
	}
	*q = '\0';
    }
    strcat(str, p);
    free(p);
}

static char *
time2str(time_t t)
{
    static char buf[128];
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", gmtime(&t));
    return buf;
}

static krb5_error_code
event2string(krb5_context context, Event *ev, char **str)
{
    char *p;
    char *pr;
    krb5_error_code ret;
    if(ev == NULL){
	*str = strdup("-");
	return (*str == NULL) ? ENOMEM : 0;
    }
    if (ev->principal == NULL) {
       pr = strdup("UNKNOWN");
       if (pr == NULL)
	   return ENOMEM;
    } else {
       ret = krb5_unparse_name(context, ev->principal, &pr);
       if(ret)
           return ret;
    }
    ret = asprintf(&p, "%s:%s", time2str(ev->time), pr);
    free(pr);
    if(ret < 0)
	return ENOMEM;
    *str = p;
    return 0;
}

krb5_error_code
hdb_entry2string(krb5_context context, hdb_entry *ent, char **str)
{
    char *p;
    char buf[1024] = "";
    char tmp[32];
    int i;
    krb5_error_code ret;

    /* --- principal */
    ret = krb5_unparse_name(context, ent->principal, &p);
    if(ret)
	return ret;
    strlcat(buf, p, sizeof(buf));
    strlcat(buf, " ", sizeof(buf));
    free(p);
    /* --- kvno */
    snprintf(tmp, sizeof(tmp), "%d", ent->kvno);
    strlcat(buf, tmp, sizeof(buf));
    /* --- keys */
    for(i = 0; i < ent->keys.len; i++){
	/* --- mkvno, keytype */
	if(ent->keys.val[i].mkvno)
	    snprintf(tmp, sizeof(tmp), ":%d:%d:", 
		     *ent->keys.val[i].mkvno, 
		     ent->keys.val[i].key.keytype);
	else
	    snprintf(tmp, sizeof(tmp), "::%d:", 
		     ent->keys.val[i].key.keytype);
	strlcat(buf, tmp, sizeof(buf));
	/* --- keydata */
	append_hex(buf, &ent->keys.val[i].key.keyvalue);
	strlcat(buf, ":", sizeof(buf));
	/* --- salt */
	if(ent->keys.val[i].salt){
	    snprintf(tmp, sizeof(tmp), "%u/", ent->keys.val[i].salt->type);
	    strlcat(buf, tmp, sizeof(buf));
	    append_hex(buf, &ent->keys.val[i].salt->salt);
	}else
	    strlcat(buf, "-", sizeof(buf));
    }
    strlcat(buf, " ", sizeof(buf));
    /* --- created by */
    event2string(context, &ent->created_by, &p);
    strlcat(buf, p, sizeof(buf));
    strlcat(buf, " ", sizeof(buf));
    free(p);
    /* --- modified by */
    event2string(context, ent->modified_by, &p);
    strlcat(buf, p, sizeof(buf));
    strlcat(buf, " ", sizeof(buf));
    free(p);

    /* --- valid start */
    if(ent->valid_start)
	strlcat(buf, time2str(*ent->valid_start), sizeof(buf));
    else
	strlcat(buf, "-", sizeof(buf));
    strlcat(buf, " ", sizeof(buf));

    /* --- valid end */
    if(ent->valid_end)
	strlcat(buf, time2str(*ent->valid_end), sizeof(buf));
    else
	strlcat(buf, "-", sizeof(buf));
    strlcat(buf, " ", sizeof(buf));
    
    /* --- password ends */
    if(ent->pw_end)
	strlcat(buf, time2str(*ent->pw_end), sizeof(buf));
    else
	strlcat(buf, "-", sizeof(buf));
    strlcat(buf, " ", sizeof(buf));

    /* --- max life */
    if(ent->max_life){
	snprintf(tmp, sizeof(tmp), "%d", *ent->max_life);
	strlcat(buf, tmp, sizeof(buf));
    }else
	strlcat(buf, "-", sizeof(buf));
    strlcat(buf, " ", sizeof(buf));

    /* --- max renewable life */
    if(ent->max_renew){
	snprintf(tmp, sizeof(tmp), "%d", *ent->max_renew);
	strlcat(buf, tmp, sizeof(buf));
    }else
	strlcat(buf, "-", sizeof(buf));
    
    strlcat(buf, " ", sizeof(buf));

    /* --- flags */
    snprintf(tmp, sizeof(tmp), "%d", HDBFlags2int(ent->flags));
    strlcat(buf, tmp, sizeof(buf));
    
    *str = strdup(buf);
    
    return 0;
}

/* print a hdb_entry to (FILE*)data; suitable for hdb_foreach */

krb5_error_code
hdb_print_entry(krb5_context context, HDB *db, hdb_entry *entry, void *data)
{
    char *p;
    hdb_entry2string(context, entry, &p);
    fprintf((FILE*)data, "%s\n", p);
    free(p);
    return 0;
}
