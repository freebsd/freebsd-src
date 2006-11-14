/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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
RCSID("$Id: config_file.c,v 1.46.4.2 2003/10/13 13:46:10 lha Exp $");

#ifndef HAVE_NETINFO

static krb5_error_code parse_section(char *p, krb5_config_section **s,
				     krb5_config_section **res,
				     const char **error_message);
static krb5_error_code parse_binding(FILE *f, unsigned *lineno, char *p,
				     krb5_config_binding **b,
				     krb5_config_binding **parent,
				     const char **error_message);
static krb5_error_code parse_list(FILE *f, unsigned *lineno,
				  krb5_config_binding **parent,
				  const char **error_message);

static krb5_config_section *
get_entry(krb5_config_section **parent, const char *name, int type)
{
    krb5_config_section **q;

    for(q = parent; *q != NULL; q = &(*q)->next)
	if(type == krb5_config_list && 
	   type == (*q)->type &&
	   strcmp(name, (*q)->name) == 0)
	    return *q;
    *q = calloc(1, sizeof(**q));
    if(*q == NULL)
	return NULL;
    (*q)->name = strdup(name);
    (*q)->type = type;
    if((*q)->name == NULL) {
	free(*q);
	*q = NULL;
	return NULL;
    }
    return *q;
}

/*
 * Parse a section:
 *
 * [section]
 *	foo = bar
 *	b = {
 *		a
 *	    }
 * ...
 * 
 * starting at the line in `p', storing the resulting structure in
 * `s' and hooking it into `parent'.
 * Store the error message in `error_message'.
 */

static krb5_error_code
parse_section(char *p, krb5_config_section **s, krb5_config_section **parent,
	      const char **error_message)
{
    char *p1;
    krb5_config_section *tmp;

    p1 = strchr (p + 1, ']');
    if (p1 == NULL) {
	*error_message = "missing ]";
	return KRB5_CONFIG_BADFORMAT;
    }
    *p1 = '\0';
    tmp = get_entry(parent, p + 1, krb5_config_list);
    if(tmp == NULL) {
	*error_message = "out of memory";
	return KRB5_CONFIG_BADFORMAT;
    }
    *s = tmp;
    return 0;
}

/*
 * Parse a brace-enclosed list from `f', hooking in the structure at
 * `parent'.
 * Store the error message in `error_message'.
 */

static krb5_error_code
parse_list(FILE *f, unsigned *lineno, krb5_config_binding **parent,
	   const char **error_message)
{
    char buf[BUFSIZ];
    krb5_error_code ret;
    krb5_config_binding *b = NULL;
    unsigned beg_lineno = *lineno;

    while(fgets(buf, sizeof(buf), f) != NULL) {
	char *p;

	++*lineno;
	if (buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';
	p = buf;
	while(isspace((unsigned char)*p))
	    ++p;
	if (*p == '#' || *p == ';' || *p == '\0')
	    continue;
	while(isspace((unsigned char)*p))
	    ++p;
	if (*p == '}')
	    return 0;
	if (*p == '\0')
	    continue;
	ret = parse_binding (f, lineno, p, &b, parent, error_message);
	if (ret)
	    return ret;
    }
    *lineno = beg_lineno;
    *error_message = "unclosed {";
    return KRB5_CONFIG_BADFORMAT;
}

/*
 *
 */

static krb5_error_code
parse_binding(FILE *f, unsigned *lineno, char *p,
	      krb5_config_binding **b, krb5_config_binding **parent,
	      const char **error_message)
{
    krb5_config_binding *tmp;
    char *p1, *p2;
    krb5_error_code ret = 0;

    p1 = p;
    while (*p && *p != '=' && !isspace((unsigned char)*p))
	++p;
    if (*p == '\0') {
	*error_message = "missing =";
	return KRB5_CONFIG_BADFORMAT;
    }
    p2 = p;
    while (isspace((unsigned char)*p))
	++p;
    if (*p != '=') {
	*error_message = "missing =";
	return KRB5_CONFIG_BADFORMAT;
    }
    ++p;
    while(isspace((unsigned char)*p))
	++p;
    *p2 = '\0';
    if (*p == '{') {
	tmp = get_entry(parent, p1, krb5_config_list);
	if (tmp == NULL) {
	    *error_message = "out of memory";
	    return KRB5_CONFIG_BADFORMAT;
	}
	ret = parse_list (f, lineno, &tmp->u.list, error_message);
    } else {
	tmp = get_entry(parent, p1, krb5_config_string);
	if (tmp == NULL) {
	    *error_message = "out of memory";
	    return KRB5_CONFIG_BADFORMAT;
	}
	p1 = p;
	p = p1 + strlen(p1);
	while(p > p1 && isspace((unsigned char)*(p-1)))
	    --p;
	*p = '\0';
	tmp->u.string = strdup(p1);
    }
    *b = tmp;
    return ret;
}

/*
 * Parse the config file `fname', generating the structures into `res'
 * returning error messages in `error_message'
 */

static krb5_error_code
krb5_config_parse_file_debug (const char *fname,
			      krb5_config_section **res,
			      unsigned *lineno,
			      const char **error_message)
{
    FILE *f;
    krb5_config_section *s;
    krb5_config_binding *b;
    char buf[BUFSIZ];
    krb5_error_code ret = 0;

    s = NULL;
    b = NULL;
    *lineno = 0;
    f = fopen (fname, "r");
    if (f == NULL) {
	*error_message = "cannot open file";
	return ENOENT;
    }
    while (fgets(buf, sizeof(buf), f) != NULL) {
	char *p;

	++*lineno;
	if(buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = '\0';
	p = buf;
	while(isspace((unsigned char)*p))
	    ++p;
	if (*p == '#' || *p == ';')
	    continue;
	if (*p == '[') {
	    ret = parse_section(p, &s, res, error_message);
	    if (ret) {
		goto out;
	    }
	    b = NULL;
	} else if (*p == '}') {
	    *error_message = "unmatched }";
	    ret = EINVAL;	/* XXX */
	    goto out;
	} else if(*p != '\0') {
	    if (s == NULL) {
		*error_message = "binding before section";
		ret = EINVAL;
		goto out;
	    }
	    ret = parse_binding(f, lineno, p, &b, &s->u.list, error_message);
	    if (ret)
		goto out;
	}
    }
out:
    fclose (f);
    return ret;
}

krb5_error_code
krb5_config_parse_file_multi (krb5_context context,
			      const char *fname,
			      krb5_config_section **res)
{
    const char *str;
    unsigned lineno;
    krb5_error_code ret;

    ret = krb5_config_parse_file_debug (fname, res, &lineno, &str);
    if (ret) {
	krb5_set_error_string (context, "%s:%u: %s", fname, lineno, str);
	return ret;
    }
    return 0;
}

krb5_error_code
krb5_config_parse_file (krb5_context context,
			const char *fname,
			krb5_config_section **res)
{
    *res = NULL;
    return krb5_config_parse_file_multi(context, fname, res);
}

#endif /* !HAVE_NETINFO */

static void
free_binding (krb5_context context, krb5_config_binding *b)
{
    krb5_config_binding *next_b;

    while (b) {
	free (b->name);
	if (b->type == krb5_config_string)
	    free (b->u.string);
	else if (b->type == krb5_config_list)
	    free_binding (context, b->u.list);
	else
	    krb5_abortx(context, "unknown binding type (%d) in free_binding", 
			b->type);
	next_b = b->next;
	free (b);
	b = next_b;
    }
}

krb5_error_code
krb5_config_file_free (krb5_context context, krb5_config_section *s)
{
    free_binding (context, s);
    return 0;
}

const void *
krb5_config_get_next (krb5_context context,
		      const krb5_config_section *c,
		      const krb5_config_binding **pointer,
		      int type,
		      ...)
{
    const char *ret;
    va_list args;

    va_start(args, type);
    ret = krb5_config_vget_next (context, c, pointer, type, args);
    va_end(args);
    return ret;
}

static const void *
vget_next(krb5_context context,
	  const krb5_config_binding *b,
	  const krb5_config_binding **pointer,
	  int type,
	  const char *name,
	  va_list args)
{
    const char *p = va_arg(args, const char *);
    while(b != NULL) {
	if(strcmp(b->name, name) == 0) {
	    if(b->type == type && p == NULL) {
		*pointer = b;
		return b->u.generic;
	    } else if(b->type == krb5_config_list && p != NULL) {
		return vget_next(context, b->u.list, pointer, type, p, args);
	    }
	}
	b = b->next;
    }
    return NULL;
}

const void *
krb5_config_vget_next (krb5_context context,
		       const krb5_config_section *c,
		       const krb5_config_binding **pointer,
		       int type,
		       va_list args)
{
    const krb5_config_binding *b;
    const char *p;

    if(c == NULL)
	c = context->cf;

    if (c == NULL)
	return NULL;

    if (*pointer == NULL) {
	/* first time here, walk down the tree looking for the right
           section */
	p = va_arg(args, const char *);
	if (p == NULL)
	    return NULL;
	return vget_next(context, c, pointer, type, p, args);
    }

    /* we were called again, so just look for more entries with the
       same name and type */
    for (b = (*pointer)->next; b != NULL; b = b->next) {
	if(strcmp(b->name, (*pointer)->name) == 0 && b->type == type) {
	    *pointer = b;
	    return b->u.generic;
	}
    }
    return NULL;
}

const void *
krb5_config_get (krb5_context context,
		 const krb5_config_section *c,
		 int type,
		 ...)
{
    const void *ret;
    va_list args;

    va_start(args, type);
    ret = krb5_config_vget (context, c, type, args);
    va_end(args);
    return ret;
}

const void *
krb5_config_vget (krb5_context context,
		  const krb5_config_section *c,
		  int type,
		  va_list args)
{
    const krb5_config_binding *foo = NULL;

    return krb5_config_vget_next (context, c, &foo, type, args);
}

const krb5_config_binding *
krb5_config_get_list (krb5_context context,
		      const krb5_config_section *c,
		      ...)
{
    const krb5_config_binding *ret;
    va_list args;

    va_start(args, c);
    ret = krb5_config_vget_list (context, c, args);
    va_end(args);
    return ret;
}

const krb5_config_binding *
krb5_config_vget_list (krb5_context context,
		       const krb5_config_section *c,
		       va_list args)
{
    return krb5_config_vget (context, c, krb5_config_list, args);
}

const char *
krb5_config_get_string (krb5_context context,
			const krb5_config_section *c,
			...)
{
    const char *ret;
    va_list args;

    va_start(args, c);
    ret = krb5_config_vget_string (context, c, args);
    va_end(args);
    return ret;
}

const char *
krb5_config_vget_string (krb5_context context,
			 const krb5_config_section *c,
			 va_list args)
{
    return krb5_config_vget (context, c, krb5_config_string, args);
}

const char *
krb5_config_vget_string_default (krb5_context context,
				 const krb5_config_section *c,
				 const char *def_value,
				 va_list args)
{
    const char *ret;

    ret = krb5_config_vget_string (context, c, args);
    if (ret == NULL)
	ret = def_value;
    return ret;
}

const char *
krb5_config_get_string_default (krb5_context context,
				const krb5_config_section *c,
				const char *def_value,
				...)
{
    const char *ret;
    va_list args;

    va_start(args, def_value);
    ret = krb5_config_vget_string_default (context, c, def_value, args);
    va_end(args);
    return ret;
}

char **
krb5_config_vget_strings(krb5_context context,
			 const krb5_config_section *c,
			 va_list args)
{
    char **strings = NULL;
    int nstr = 0;
    const krb5_config_binding *b = NULL;
    const char *p;

    while((p = krb5_config_vget_next(context, c, &b, 
				     krb5_config_string, args))) {
	char *tmp = strdup(p);
	char *pos = NULL;
	char *s;
	if(tmp == NULL)
	    goto cleanup;
	s = strtok_r(tmp, " \t", &pos);
	while(s){
	    char **tmp = realloc(strings, (nstr + 1) * sizeof(*strings));
	    if(tmp == NULL)
		goto cleanup;
	    strings = tmp;
	    strings[nstr] = strdup(s);
	    nstr++;
	    if(strings[nstr-1] == NULL)
		goto cleanup;
	    s = strtok_r(NULL, " \t", &pos);
	}
	free(tmp);
    }
    if(nstr){
	char **tmp = realloc(strings, (nstr + 1) * sizeof(*strings));
	if(strings == NULL)
	    goto cleanup;
	strings = tmp;
	strings[nstr] = NULL;
    }
    return strings;
cleanup:
    while(nstr--)
	free(strings[nstr]);
    free(strings);
    return NULL;

}

char**
krb5_config_get_strings(krb5_context context,
			const krb5_config_section *c,
			...)
{
    va_list ap;
    char **ret;
    va_start(ap, c);
    ret = krb5_config_vget_strings(context, c, ap);
    va_end(ap);
    return ret;
}

void
krb5_config_free_strings(char **strings)
{
    char **s = strings;
    while(s && *s){
	free(*s);
	s++;
    }
    free(strings);
}

krb5_boolean
krb5_config_vget_bool_default (krb5_context context,
			       const krb5_config_section *c,
			       krb5_boolean def_value,
			       va_list args)
{
    const char *str;
    str = krb5_config_vget_string (context, c, args);
    if(str == NULL)
	return def_value;
    if(strcasecmp(str, "yes") == 0 ||
       strcasecmp(str, "true") == 0 ||
       atoi(str)) return TRUE;
    return FALSE;
}

krb5_boolean
krb5_config_vget_bool  (krb5_context context,
			const krb5_config_section *c,
			va_list args)
{
    return krb5_config_vget_bool_default (context, c, FALSE, args);
}

krb5_boolean
krb5_config_get_bool_default (krb5_context context,
			      const krb5_config_section *c,
			      krb5_boolean def_value,
			      ...)
{
    va_list ap;
    krb5_boolean ret;
    va_start(ap, def_value);
    ret = krb5_config_vget_bool_default(context, c, def_value, ap);
    va_end(ap);
    return ret;
}

krb5_boolean
krb5_config_get_bool (krb5_context context,
		      const krb5_config_section *c,
		      ...)
{
    va_list ap;
    krb5_boolean ret;
    va_start(ap, c);
    ret = krb5_config_vget_bool (context, c, ap);
    va_end(ap);
    return ret;
}

int
krb5_config_vget_time_default (krb5_context context,
			       const krb5_config_section *c,
			       int def_value,
			       va_list args)
{
    const char *str;
    str = krb5_config_vget_string (context, c, args);
    if(str == NULL)
	return def_value;
    return parse_time (str, NULL);
}

int
krb5_config_vget_time  (krb5_context context,
			const krb5_config_section *c,
			va_list args)
{
    return krb5_config_vget_time_default (context, c, -1, args);
}

int
krb5_config_get_time_default (krb5_context context,
			      const krb5_config_section *c,
			      int def_value,
			      ...)
{
    va_list ap;
    int ret;
    va_start(ap, def_value);
    ret = krb5_config_vget_time_default(context, c, def_value, ap);
    va_end(ap);
    return ret;
}

int
krb5_config_get_time (krb5_context context,
		      const krb5_config_section *c,
		      ...)
{
    va_list ap;
    int ret;
    va_start(ap, c);
    ret = krb5_config_vget_time (context, c, ap);
    va_end(ap);
    return ret;
}


int
krb5_config_vget_int_default (krb5_context context,
			      const krb5_config_section *c,
			      int def_value,
			      va_list args)
{
    const char *str;
    str = krb5_config_vget_string (context, c, args);
    if(str == NULL)
	return def_value;
    else { 
	char *endptr; 
	long l; 
	l = strtol(str, &endptr, 0); 
	if (endptr == str) 
	    return def_value; 
	else 
	    return l;
    }
}

int
krb5_config_vget_int  (krb5_context context,
		       const krb5_config_section *c,
		       va_list args)
{
    return krb5_config_vget_int_default (context, c, -1, args);
}

int
krb5_config_get_int_default (krb5_context context,
			     const krb5_config_section *c,
			     int def_value,
			     ...)
{
    va_list ap;
    int ret;
    va_start(ap, def_value);
    ret = krb5_config_vget_int_default(context, c, def_value, ap);
    va_end(ap);
    return ret;
}

int
krb5_config_get_int (krb5_context context,
		     const krb5_config_section *c,
		     ...)
{
    va_list ap;
    int ret;
    va_start(ap, c);
    ret = krb5_config_vget_int (context, c, ap);
    va_end(ap);
    return ret;
}
