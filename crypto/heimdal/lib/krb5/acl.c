/*
 * Copyright (c) 2000 - 2001 Kungliga Tekniska Högskolan
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
#include <fnmatch.h>

RCSID("$Id: acl.c,v 1.2 2001/05/14 06:14:43 assar Exp $");

struct acl_field {
    enum { acl_string, acl_fnmatch, acl_retval } type;
    union {
	const char *cstr;
	char **retv;
    } u;
    struct acl_field *next, **last;
};

static void
acl_free_list(struct acl_field *acl)
{
    struct acl_field *next;
    while(acl != NULL) {
	next = acl->next;
	free(acl);
	acl = next;
    }
}

static krb5_error_code
acl_parse_format(krb5_context context,
		 struct acl_field **acl_ret,
		 const char *format,
		 va_list ap)
{
    const char *p;
    struct acl_field *acl = NULL, *tmp;

    for(p = format; *p != '\0'; p++) {
	tmp = malloc(sizeof(*tmp));
	if(tmp == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    acl_free_list(acl);
	    return ENOMEM;
	}
	if(*p == 's') {
	    tmp->type = acl_string;
	    tmp->u.cstr = va_arg(ap, const char*);
	} else if(*p == 'f') {
	    tmp->type = acl_fnmatch;
	    tmp->u.cstr = va_arg(ap, const char*);
	} else if(*p == 'r') {
	    tmp->type = acl_retval;
	    tmp->u.retv = va_arg(ap, char **);
	}
	tmp->next = NULL;
	if(acl == NULL)
	    acl = tmp;
	else
	    *acl->last = tmp;
	acl->last = &tmp->next;
    }
    *acl_ret = acl;
    return 0;
}

static krb5_boolean
acl_match_field(krb5_context context,
		const char *string,
		struct acl_field *field)
{
    if(field->type == acl_string) {
	return !strcmp(string, field->u.cstr);
    } else if(field->type == acl_fnmatch) {
	return !fnmatch(string, field->u.cstr, 0);
    } else if(field->type == acl_retval) {
	*field->u.retv = strdup(string);
	return TRUE;
    }
    return FALSE;
}

static krb5_boolean
acl_match_acl(krb5_context context,
	      struct acl_field *acl,
	      const char *string)
{
    char buf[256];
    for(;strsep_copy(&string, " \t", buf, sizeof(buf)) != -1; 
	acl = acl->next) {
	if(buf[0] == '\0')
	    continue; /* skip ws */
	if(!acl_match_field(context, buf, acl)) {
	    return FALSE;
	}
    }
    return TRUE;
}


krb5_error_code
krb5_acl_match_string(krb5_context context,
		      const char *acl_string,
		      const char *format,
		      ...)
{
    krb5_error_code ret;
    krb5_boolean found;
    struct acl_field *acl;

    va_list ap;
    va_start(ap, format);
    ret = acl_parse_format(context, &acl, format, ap);
    va_end(ap);
    if(ret)
	return ret;

    found = acl_match_acl(context, acl, acl_string);
    acl_free_list(acl);
    if (found) {
	return 0;
    } else {
	krb5_set_error_string(context, "ACL did not match");
	return EACCES;
    }
}
	       
krb5_error_code
krb5_acl_match_file(krb5_context context,
		    const char *file,
		    const char *format,
		    ...)
{
    krb5_error_code ret;
    struct acl_field *acl;
    char buf[256];
    va_list ap;
    FILE *f;
    krb5_boolean found;

    f = fopen(file, "r");
    if(f == NULL) {
	int save_errno = errno;

	krb5_set_error_string(context, "open(%s): %s", file,
			      strerror(save_errno));
	return save_errno;
    }

    va_start(ap, format);
    ret = acl_parse_format(context, &acl, format, ap);
    va_end(ap);
    if(ret) {
	fclose(f);
	return ret;
    }

    found = FALSE;
    while(fgets(buf, sizeof(buf), f)) {
	if(buf[0] == '#')
	    continue;
	if(acl_match_acl(context, acl, buf)) {
	    found = TRUE;
	    break;
	}
    }

    fclose(f);
    acl_free_list(acl);
    if (found) {
	return 0;
    } else {
	krb5_set_error_string(context, "ACL did not match");
	return EACCES;
    }
}
