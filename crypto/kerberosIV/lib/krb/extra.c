/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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

#include "krb_locl.h"

RCSID("$Id: extra.c,v 1.7.2.1 2000/12/07 16:06:09 assar Exp $");

struct value {
    char *variable;
    char *value;
    struct value *next;
};

static struct value *_extra_values;

static int _krb_extra_read = 0;

static int
define_variable(const char *variable, const char *value)
{
    struct value *e;
    e = malloc(sizeof(*e));
    if(e == NULL)
	return ENOMEM;
    e->variable = strdup(variable);
    if(e->variable == NULL) {
	free(e);
	return ENOMEM;
    }
    e->value = strdup(value);
    if(e->value == NULL) {
	free(e->variable);
	free(e);
	return ENOMEM;
    }
    e->next = _extra_values;
    _extra_values = e;
    return 0;
}

#ifndef WIN32

static int
read_extra_file(void)
{
    int i = 0;
    char file[128];
    char line[1024];
    if(_krb_extra_read)
	return 0;
    _krb_extra_read = 1;
    while(krb_get_krbextra(i++, file, sizeof(file)) == 0) {
	FILE *f = fopen(file, "r");
	if(f == NULL)
	    continue;
	while(fgets(line, sizeof(line), f)) {
	    char *var, *tmp, *val;

	    /* skip initial whitespace */
	    var = line + strspn(line, " \t");
	    /* skip non-whitespace */
	    tmp = var + strcspn(var, " \t=");
	    /* skip whitespace */
	    val = tmp + strspn(tmp, " \t=");
	    *tmp = '\0';
	    tmp = val + strcspn(val, " \t\n");
	    *tmp = '\0';
	    if(*var == '\0' || *var == '#' || *val == '\0')
		continue;
	    if(krb_debug)
		krb_warning("%s: setting `%s' to `%s'\n", file, var, val);
	    define_variable(var, val);
	}
	fclose(f);
	return 0;
    }
    return ENOENT;
}

#else /* WIN32 */

static int
read_extra_file(void)
{
    char name[1024], data[1024];
    DWORD name_sz, data_sz;
    DWORD type;
    int num = 0;
    HKEY reg_key;

    if(_krb_extra_read)
	return 0;
    _krb_extra_read = 1;

    if(RegCreateKey(HKEY_CURRENT_USER, "krb4", &reg_key) != 0)
	return -1;
	

    while(1) {
	name_sz = sizeof(name);
	data_sz = sizeof(data);
	if(RegEnumValue(reg_key,
			num++,
			name,
			&name_sz,
			NULL,
			&type,
			data,
			&data_sz) != 0)
	    break;
	if(type == REG_SZ)
	    define_variable(name, data);
    }
    RegCloseKey(reg_key);
    return 0;
}

#endif

static const char*
find_variable(const char *variable)
{
    struct value *e;
    for(e = _extra_values; e; e = e->next) {
	if(strcasecmp(variable, e->variable) == 0)
	    return e->value;
    }
    return NULL;
}

const char *
krb_get_config_string(const char *variable)
{
    read_extra_file();
    return find_variable(variable);
}

int
krb_get_config_bool(const char *variable)
{
    const char *value = krb_get_config_string(variable);
    if(value == NULL)
	return 0;
    return strcasecmp(value, "yes") == 0 || 
	strcasecmp(value, "true") == 0 ||
	atoi(value);
}
