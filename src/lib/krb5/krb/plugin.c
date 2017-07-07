/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/plugin.c - Plugin framework functions */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"

/*
 * A plugin_mapping structure maps a module name to a built-in or dynamic
 * module.  modname is always present; the other three fields can be in four
 * different states:
 *
 * - If dyn_path and dyn_handle are null but module is set, the mapping is to a
 *   built-in module.
 * - If dyn_path is set but dyn_handle and module are null, the mapping is to a
 *   dynamic module which hasn't been loaded yet.
 * - If all three fields are set, the mapping is to a dynamic module which has
 *   been loaded and is ready to use.
 * - If all three fields are null, the mapping is to a dynamic module which
 *   failed to load and should be ignored.
 */
struct plugin_mapping {
    char *modname;
    char *dyn_path;
    struct plugin_file_handle *dyn_handle;
    krb5_plugin_initvt_fn module;
};

const char *interface_names[] = {
    "pwqual",
    "kadm5_hook",
    "clpreauth",
    "kdcpreauth",
    "ccselect",
    "localauth",
    "hostrealm",
    "audit",
    "tls",
    "kdcauthdata"
};

/* Return the context's interface structure for id, or NULL if invalid. */
static inline struct plugin_interface *
get_interface(krb5_context context, int id)
{
    if (context == NULL || id < 0 || id >= PLUGIN_NUM_INTERFACES)
        return NULL;
    return &context->plugins[id];
}

/* Release the memory associated with the mapping list entry map. */
static void
free_plugin_mapping(struct plugin_mapping *map)
{
    if (map == NULL)
        return;
    free(map->modname);
    free(map->dyn_path);
    if (map->dyn_handle != NULL)
        krb5int_close_plugin(map->dyn_handle);
    free(map);
}

static void
free_mapping_list(struct plugin_mapping **list)
{
    struct plugin_mapping **mp;

    for (mp = list; mp != NULL && *mp != NULL; mp++)
        free_plugin_mapping(*mp);
    free(list);
}

/* Construct a plugin mapping object.  path may be NULL (for a built-in
 * module), or may be relative to the plugin base directory. */
static krb5_error_code
make_plugin_mapping(krb5_context context, const char *name, size_t namelen,
                    const char *path, krb5_plugin_initvt_fn module,
                    struct plugin_mapping **map_out)
{
    krb5_error_code ret;
    struct plugin_mapping *map = NULL;

    /* Create the mapping entry. */
    map = k5alloc(sizeof(*map), &ret);
    if (map == NULL)
        return ret;

    map->modname = k5memdup0(name, namelen, &ret);
    if (map->modname == NULL)
        goto oom;
    if (path != NULL) {
        if (k5_path_join(context->plugin_base_dir, path, &map->dyn_path))
            goto oom;
    }
    map->module = module;
    *map_out = map;
    return 0;

oom:
    free_plugin_mapping(map);
    return ENOMEM;
}

/*
 * Register a mapping from modname to either dyn_path (for an auto-registered
 * dynamic module) or to module (for a builtin module).  dyn_path may be
 * relative to the plugin base directory.
 */
static krb5_error_code
register_module(krb5_context context, struct plugin_interface *interface,
                const char *modname, const char *dyn_path,
                krb5_plugin_initvt_fn module)
{
    struct plugin_mapping **list;
    size_t count;

    /* Allocate list space for another element and a terminator. */
    list = interface->modules;
    for (count = 0; list != NULL && list[count] != NULL; count++);
    list = realloc(interface->modules, (count + 2) * sizeof(*list));
    if (list == NULL)
        return ENOMEM;
    list[count] = list[count + 1] = NULL;
    interface->modules = list;

    /* Create a new mapping structure and add it to the list. */
    return make_plugin_mapping(context, modname, strlen(modname), dyn_path,
                               module, &list[count]);
}

/* Parse a profile module string of the form "modname:modpath" into a mapping
 * entry. */
static krb5_error_code
parse_modstr(krb5_context context, const char *modstr,
             struct plugin_mapping **map_out)
{
    const char *sep;

    *map_out = NULL;

    sep = strchr(modstr, ':');
    if (sep == NULL) {
        k5_setmsg(context, KRB5_PLUGIN_BAD_MODULE_SPEC,
                  _("Invalid module specifier %s"), modstr);
        return KRB5_PLUGIN_BAD_MODULE_SPEC;
    }

    return make_plugin_mapping(context, modstr, sep - modstr, sep + 1, NULL,
                               map_out);
}

/* Return true if value is found in list. */
static krb5_boolean
find_in_list(char **list, const char *value)
{
    for (; *list != NULL; list++) {
        if (strcmp(*list, value) == 0)
            return TRUE;
    }
    return FALSE;
}

/* Get the list of values for the profile variable varname in the section for
 * interface id, or NULL if no values are set. */
static krb5_error_code
get_profile_var(krb5_context context, int id, const char *varname, char ***out)
{
    krb5_error_code ret;
    const char *path[4];

    *out = NULL;
    path[0] = KRB5_CONF_PLUGINS;
    path[1] = interface_names[id];
    path[2] = varname;
    path[3] = NULL;
    ret = profile_get_values(context->profile, path, out);
    return (ret == PROF_NO_RELATION) ? 0 : ret;
}

/* Expand *list_inout to contain the mappings from modstrs, followed by the
 * existing built-in module mappings. */
static krb5_error_code
make_full_list(krb5_context context, char **modstrs,
               struct plugin_mapping ***list_inout)
{
    krb5_error_code ret = 0;
    size_t count, pos, i, j;
    struct plugin_mapping **list, **mp;
    char **mod;

    /* Allocate space for all of the modules plus a null terminator. */
    for (count = 0; modstrs[count] != NULL; count++);
    for (mp = *list_inout; mp != NULL && *mp != NULL; mp++, count++);
    list = calloc(count + 1, sizeof(*list));
    if (list == NULL)
        return ENOMEM;

    /* Parse each profile module entry and store it in the list. */
    for (mod = modstrs, pos = 0; *mod != NULL; mod++, pos++) {
        ret = parse_modstr(context, *mod, &list[pos]);
        if (ret != 0) {
            free_mapping_list(list);
            return ret;
        }
    }

    /* Cannibalize the old list of built-in modules. */
    for (mp = *list_inout; mp != NULL && *mp != NULL; mp++, pos++)
        list[pos] = *mp;
    assert(pos == count);

    /* Filter out duplicates, preferring earlier entries to later ones. */
    for (i = 0, pos = 0; i < count; i++) {
        for (j = 0; j < pos; j++) {
            if (strcmp(list[i]->modname, list[j]->modname) == 0) {
                free_plugin_mapping(list[i]);
                break;
            }
        }
        if (j == pos)
            list[pos++] = list[i];
    }
    list[pos] = NULL;

    free(*list_inout);
    *list_inout = list;
    return 0;
}

/* Remove any entries from list which match values in disabled. */
static void
remove_disabled_modules(struct plugin_mapping **list, char **disable)
{
    struct plugin_mapping **in, **out;

    out = list;
    for (in = list; *in != NULL; in++) {
        if (find_in_list(disable, (*in)->modname))
            free_plugin_mapping(*in);
        else
            *out++ = *in;
    }
    *out = NULL;
}

/* Modify list to include only the entries matching strings in enable, in
 * the order they are listed there. */
static void
filter_enabled_modules(struct plugin_mapping **list, char **enable)
{
    size_t count, i, pos = 0;
    struct plugin_mapping *tmp;

    /* Count the number of existing entries. */
    for (count = 0; list[count] != NULL; count++);

    /* For each string in enable, look for a matching module. */
    for (; *enable != NULL; enable++) {
        for (i = pos; i < count; i++) {
            if (strcmp(list[i]->modname, *enable) == 0) {
                /* Swap the matching module into the next result position. */
                tmp = list[pos];
                list[pos++] = list[i];
                list[i] = tmp;
                break;
            }
        }
    }

    /* Free all mappings which didn't match and terminate the list. */
    for (i = pos; i < count; i++)
        free_plugin_mapping(list[i]);
    list[pos] = NULL;
}

/* Ensure that a plugin interface is configured.  id must be valid. */
static krb5_error_code
configure_interface(krb5_context context, int id)
{
    krb5_error_code ret;
    struct plugin_interface *interface = &context->plugins[id];
    char **modstrs = NULL, **enable = NULL, **disable = NULL;

    if (interface->configured)
        return 0;

    /* Detect consistency errors when plugin interfaces are added. */
    assert(sizeof(interface_names) / sizeof(*interface_names) ==
           PLUGIN_NUM_INTERFACES);

    /* Get profile variables for this interface. */
    ret = get_profile_var(context, id, KRB5_CONF_MODULE, &modstrs);
    if (ret)
        goto cleanup;
    ret = get_profile_var(context, id, KRB5_CONF_DISABLE, &disable);
    if (ret)
        goto cleanup;
    ret = get_profile_var(context, id, KRB5_CONF_ENABLE_ONLY, &enable);
    if (ret)
        goto cleanup;

    /* Create the full list of dynamic and built-in modules. */
    if (modstrs != NULL) {
        ret = make_full_list(context, modstrs, &interface->modules);
        if (ret)
            goto cleanup;
    }

    /* Remove disabled modules. */
    if (disable != NULL)
        remove_disabled_modules(interface->modules, disable);

    /* Filter and re-order the list according to enable-modules. */
    if (enable != NULL)
        filter_enabled_modules(interface->modules, enable);

cleanup:
    profile_free_list(modstrs);
    profile_free_list(enable);
    profile_free_list(disable);
    return ret;
}

/* If map is for a dynamic module which hasn't been loaded yet, attempt to load
 * it.  Only try to load a module once. */
static void
load_if_needed(krb5_context context, struct plugin_mapping *map,
               const char *iname)
{
    char *symname = NULL;
    struct plugin_file_handle *handle = NULL;
    void (*initvt_fn)();

    if (map->module != NULL || map->dyn_path == NULL)
        return;
    if (asprintf(&symname, "%s_%s_initvt", iname, map->modname) < 0)
        return;
    if (krb5int_open_plugin(map->dyn_path, &handle, &context->err))
        goto err;
    if (krb5int_get_plugin_func(handle, symname, &initvt_fn, &context->err))
        goto err;
    free(symname);
    map->dyn_handle = handle;
    map->module = (krb5_plugin_initvt_fn)initvt_fn;
    return;

err:
    /* Clean up, and also null out map->dyn_path so we don't try again. */
    if (handle != NULL)
        krb5int_close_plugin(handle);
    free(symname);
    free(map->dyn_path);
    map->dyn_path = NULL;
}

krb5_error_code
k5_plugin_load(krb5_context context, int interface_id, const char *modname,
               krb5_plugin_initvt_fn *module)
{
    krb5_error_code ret;
    struct plugin_interface *interface = get_interface(context, interface_id);
    struct plugin_mapping **mp, *map;

    if (interface == NULL)
        return EINVAL;
    ret = configure_interface(context, interface_id);
    if (ret != 0)
        return ret;
    for (mp = interface->modules; mp != NULL && *mp != NULL; mp++) {
        map = *mp;
        if (strcmp(map->modname, modname) == 0) {
            load_if_needed(context, map, interface_names[interface_id]);
            if (map->module != NULL) {
                *module = map->module;
                return 0;
            }
            break;
        }
    }
    k5_setmsg(context, KRB5_PLUGIN_NAME_NOTFOUND,
              _("Could not find %s plugin module named '%s'"),
              interface_names[interface_id], modname);
    return KRB5_PLUGIN_NAME_NOTFOUND;
}

krb5_error_code
k5_plugin_load_all(krb5_context context, int interface_id,
                   krb5_plugin_initvt_fn **modules)
{
    krb5_error_code ret;
    struct plugin_interface *interface = get_interface(context, interface_id);
    struct plugin_mapping **mp, *map;
    krb5_plugin_initvt_fn *list;
    size_t count;

    if (interface == NULL)
        return EINVAL;
    ret = configure_interface(context, interface_id);
    if (ret != 0)
        return ret;

    /* Count the modules and allocate a list to hold them. */
    mp = interface->modules;
    for (count = 0; mp != NULL && mp[count] != NULL; count++);
    list = calloc(count + 1, sizeof(*list));
    if (list == NULL)
        return ENOMEM;

    /* Place each module's initvt function into list. */
    count = 0;
    for (mp = interface->modules; mp != NULL && *mp != NULL; mp++) {
        map = *mp;
        load_if_needed(context, map, interface_names[interface_id]);
        if (map->module != NULL)
            list[count++] = map->module;
    }

    *modules = list;
    return 0;
}

void
k5_plugin_free_modules(krb5_context context, krb5_plugin_initvt_fn *modules)
{
    free(modules);
}

krb5_error_code
k5_plugin_register(krb5_context context, int interface_id, const char *modname,
                   krb5_plugin_initvt_fn module)
{
    struct plugin_interface *interface = get_interface(context, interface_id);

    if (interface == NULL)
        return EINVAL;

    /* Disallow registering plugins after load.  We may need to reconsider
     * this, but it simplifies the design. */
    if (interface->configured)
        return EINVAL;

    return register_module(context, interface, modname, NULL, module);
}

krb5_error_code
k5_plugin_register_dyn(krb5_context context, int interface_id,
                       const char *modname, const char *modsubdir)
{
    krb5_error_code ret;
    struct plugin_interface *interface = get_interface(context, interface_id);
    char *path;

    /* Disallow registering plugins after load. */
    if (interface == NULL || interface->configured)
        return EINVAL;

    if (asprintf(&path, "%s/%s%s", modsubdir, modname, PLUGIN_EXT) < 0)
        return ENOMEM;
    ret = register_module(context, interface, modname, path, NULL);
    free(path);
    return ret;
}

void
k5_plugin_free_context(krb5_context context)
{
    int i;

    for (i = 0; i < PLUGIN_NUM_INTERFACES; i++)
        free_mapping_list(context->plugins[i].modules);
    memset(context->plugins, 0, sizeof(context->plugins));
}
