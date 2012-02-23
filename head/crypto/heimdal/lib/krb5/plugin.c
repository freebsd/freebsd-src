/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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
RCSID("$Id: plugin.c 22033 2007-11-10 10:39:47Z lha $");
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <dirent.h>

struct krb5_plugin {
    void *symbol;
    void *dsohandle;
    struct krb5_plugin *next;
};

struct plugin {
    enum krb5_plugin_type type;
    void *name;
    void *symbol;
    struct plugin *next;
};

static HEIMDAL_MUTEX plugin_mutex = HEIMDAL_MUTEX_INITIALIZER;
static struct plugin *registered = NULL;

static const char *plugin_dir = LIBDIR "/plugin/krb5";

/*
 *
 */

void *
_krb5_plugin_get_symbol(struct krb5_plugin *p)
{
    return p->symbol;
}

struct krb5_plugin *
_krb5_plugin_get_next(struct krb5_plugin *p)
{
    return p->next;
}

/*
 *
 */

#ifdef HAVE_DLOPEN

static krb5_error_code
loadlib(krb5_context context,
	enum krb5_plugin_type type,
	const char *name,
	const char *lib,
	struct krb5_plugin **e)
{
    *e = calloc(1, sizeof(**e));
    if (*e == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

    (*e)->dsohandle = dlopen(lib, RTLD_LAZY);
    if ((*e)->dsohandle == NULL) {
	free(*e);
	*e = NULL;
	krb5_set_error_string(context, "Failed to load %s: %s", 
			      lib, dlerror());
	return ENOMEM;
    }

    /* dlsym doesn't care about the type */
    (*e)->symbol = dlsym((*e)->dsohandle, name);
    if ((*e)->symbol == NULL) {
	dlclose((*e)->dsohandle);
	free(*e);
	krb5_clear_error_string(context);
	return ENOMEM;
    }

    return 0;
}
#endif /* HAVE_DLOPEN */

/**
 * Register a plugin symbol name of specific type.
 * @param context a Keberos context
 * @param type type of plugin symbol
 * @param name name of plugin symbol
 * @param symbol a pointer to the named symbol
 * @return In case of error a non zero error com_err error is returned
 * and the Kerberos error string is set.
 *
 * @ingroup krb5_support
 */

krb5_error_code
krb5_plugin_register(krb5_context context,
		     enum krb5_plugin_type type,
		     const char *name, 
		     void *symbol)
{
    struct plugin *e;

    e = calloc(1, sizeof(*e));
    if (e == NULL) {
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    e->type = type;
    e->name = strdup(name);
    if (e->name == NULL) {
	free(e);
	krb5_set_error_string(context, "out of memory");
	return ENOMEM;
    }
    e->symbol = symbol;

    HEIMDAL_MUTEX_lock(&plugin_mutex);
    e->next = registered;
    registered = e;
    HEIMDAL_MUTEX_unlock(&plugin_mutex);

    return 0;
}

krb5_error_code
_krb5_plugin_find(krb5_context context,
		  enum krb5_plugin_type type,
		  const char *name, 
		  struct krb5_plugin **list)
{
    struct krb5_plugin *e;
    struct plugin *p;
    krb5_error_code ret;
    char *sysdirs[2] = { NULL, NULL };
    char **dirs = NULL, **di;
    struct dirent *entry;
    char *path;
    DIR *d = NULL;

    *list = NULL;

    HEIMDAL_MUTEX_lock(&plugin_mutex);

    for (p = registered; p != NULL; p = p->next) {
	if (p->type != type || strcmp(p->name, name) != 0)
	    continue;

	e = calloc(1, sizeof(*e));
	if (e == NULL) {
	    HEIMDAL_MUTEX_unlock(&plugin_mutex);
	    krb5_set_error_string(context, "out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	e->symbol = p->symbol;
	e->dsohandle = NULL;
	e->next = *list;
	*list = e;
    }
    HEIMDAL_MUTEX_unlock(&plugin_mutex);

#ifdef HAVE_DLOPEN

    dirs = krb5_config_get_strings(context, NULL, "libdefaults", 
				   "plugin_dir", NULL);
    if (dirs == NULL) {
	sysdirs[0] = rk_UNCONST(plugin_dir);
	dirs = sysdirs;
    }

    for (di = dirs; *di != NULL; di++) {

	d = opendir(*di);
	if (d == NULL)
	    continue;

	while ((entry = readdir(d)) != NULL) {
	    asprintf(&path, "%s/%s", *di, entry->d_name);
	    if (path == NULL) {
		krb5_set_error_string(context, "out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    ret = loadlib(context, type, name, path, &e);
	    free(path);
	    if (ret)
		continue;
	    
	    e->next = *list;
	    *list = e;
	}
	closedir(d);
    }
    if (dirs != sysdirs)
	krb5_config_free_strings(dirs);
#endif /* HAVE_DLOPEN */

    if (*list == NULL) {
	krb5_set_error_string(context, "Did not find a plugin for %s", name);
	return ENOENT;
    }

    return 0;

out:
    if (dirs && dirs != sysdirs)
	krb5_config_free_strings(dirs);
    if (d)
	closedir(d);
    _krb5_plugin_free(*list);
    *list = NULL;

    return ret;
}

void
_krb5_plugin_free(struct krb5_plugin *list)
{
    struct krb5_plugin *next;
    while (list) {
	next = list->next;
	if (list->dsohandle)
	    dlclose(list->dsohandle);
	free(list);
	list = next;
    }
}
