/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * prof_get.c --- routines that expose the public interfaces for
 *      querying items from the profile.
 *
 */

#include "prof_int.h"
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#include <limits.h>

/*
 * These functions --- init_list(), end_list(), and add_to_list() are
 * internal functions used to build up a null-terminated char ** list
 * of strings to be returned by functions like profile_get_values.
 *
 * The profile_string_list structure is used for internal booking
 * purposes to build up the list, which is returned in *ret_list by
 * the end_list() function.
 *
 * The publicly exported interface for freeing char** list is
 * profile_free_list().
 */

struct profile_string_list {
    char    **list;
    unsigned int    num;
    unsigned int    max;
};

/*
 * Initialize the string list abstraction.
 */
static errcode_t init_list(struct profile_string_list *list)
{
    list->num = 0;
    list->max = 10;
    list->list = malloc(list->max * sizeof(char *));
    if (list->list == 0)
        return ENOMEM;
    list->list[0] = 0;
    return 0;
}

/*
 * Free any memory left over in the string abstraction, returning the
 * built up list in *ret_list if it is non-null.
 */
static void end_list(struct profile_string_list *list, char ***ret_list)
{
    char    **cp;

    if (list == 0)
        return;

    if (ret_list) {
        *ret_list = list->list;
        return;
    } else {
        for (cp = list->list; cp && *cp; cp++)
            free(*cp);
        free(list->list);
    }
    list->num = list->max = 0;
    list->list = 0;
}

/*
 * Add a string to the list.
 */
static errcode_t add_to_list(struct profile_string_list *list, const char *str)
{
    char    *newstr, **newlist;
    unsigned int    newmax;

    if (list->num+1 >= list->max) {
        newmax = list->max + 10;
        newlist = realloc(list->list, newmax * sizeof(char *));
        if (newlist == 0)
            return ENOMEM;
        list->max = newmax;
        list->list = newlist;
    }
    newstr = strdup(str);
    if (newstr == 0)
        return ENOMEM;

    list->list[list->num++] = newstr;
    list->list[list->num] = 0;
    return 0;
}

/*
 * Return TRUE if the string is already a member of the list.
 */
static int is_list_member(struct profile_string_list *list, const char *str)
{
    char **cpp;

    if (!list->list)
        return 0;

    for (cpp = list->list; *cpp; cpp++) {
        if (!strcmp(*cpp, str))
            return 1;
    }
    return 0;
}

/*
 * This function frees a null-terminated list as returned by
 * profile_get_values.
 */
void KRB5_CALLCONV profile_free_list(char **list)
{
    char        **cp;

    if (list == 0)
        return;

    for (cp = list; *cp; cp++)
        free(*cp);
    free(list);
}

/* Look up a relation in a vtable profile. */
static errcode_t
get_values_vt(profile_t profile, const char *const *names, char ***ret_values)
{
    errcode_t               retval;
    char                    **vtvalues, **val;
    struct profile_string_list values;

    retval = profile->vt->get_values(profile->cbdata, names, &vtvalues);
    if (retval)
        return retval;

    /* Copy the result into memory we can free. */
    retval = init_list(&values);
    if (retval == 0) {
        for (val = vtvalues; *val; val++)
            add_to_list(&values, *val);
        end_list(&values, ret_values);
    }

    profile->vt->free_values(profile->cbdata, vtvalues);
    return retval;
}

errcode_t KRB5_CALLCONV
profile_get_values(profile_t profile, const char *const *names,
                   char ***ret_values)
{
    errcode_t               retval;
    void                    *state = NULL;
    char                    *value;
    struct profile_string_list values;

    *ret_values = NULL;
    if (!profile)
        return PROF_NO_PROFILE;
    if (profile->vt)
        return get_values_vt(profile, names, ret_values);

    if ((retval = profile_node_iterator_create(profile, names,
                                               PROFILE_ITER_RELATIONS_ONLY,
                                               &state)))
        return retval;

    retval = init_list(&values);
    if (retval)
        goto cleanup;

    do {
        if ((retval = profile_node_iterator(&state, 0, 0, &value)))
            goto cleanup;
        if (value)
            add_to_list(&values, value);
    } while (state);

    if (values.num == 0) {
        retval = PROF_NO_RELATION;
        goto cleanup;
    }

cleanup:
    end_list(&values, retval ? NULL : ret_values);
    profile_node_iterator_free(&state);
    return retval;
}

/* Look up a relation in a vtable profile and return the first value in the
 * result. */
static errcode_t
get_value_vt(profile_t profile, const char *const *names, char **ret_value)
{
    errcode_t               retval;
    char                    **vtvalues;

    retval = profile->vt->get_values(profile->cbdata, names, &vtvalues);
    if (retval)
        return retval;
    *ret_value = strdup(*vtvalues);
    if (*ret_value == NULL)
        retval = ENOMEM;
    profile->vt->free_values(profile->cbdata, vtvalues);
    return retval;
}

/*
 * This function only gets the first value from the file; it is a
 * helper function for profile_get_string, profile_get_integer, etc.
 */
errcode_t profile_get_value(profile_t profile, const char **names,
                            char **ret_value)
{
    errcode_t               retval;
    void                    *state;
    char                    *value;

    *ret_value = NULL;
    if (!profile)
        return PROF_NO_PROFILE;
    if (profile->vt)
        return get_value_vt(profile, names, ret_value);

    retval = profile_iterator_create(profile, names,
                                     PROFILE_ITER_RELATIONS_ONLY, &state);
    if (retval)
        return retval;

    retval = profile_iterator(&state, NULL, &value);
    if (retval)
        goto cleanup;

    if (value)
        *ret_value = value;
    else
        retval = PROF_NO_RELATION;

cleanup:
    profile_iterator_free(&state);
    return retval;
}

errcode_t KRB5_CALLCONV
profile_get_string(profile_t profile, const char *name, const char *subname,
                   const char *subsubname, const char *def_val,
                   char **ret_string)
{
    char            *value;
    errcode_t       retval;
    const char      *names[4];

    if (profile) {
        names[0] = name;
        names[1] = subname;
        names[2] = subsubname;
        names[3] = 0;
        retval = profile_get_value(profile, names, &value);
        if (retval == 0) {
            *ret_string = value;
            return 0;
        } else if (retval != PROF_NO_SECTION && retval != PROF_NO_RELATION)
            return retval;
    }

    if (def_val) {
        *ret_string = strdup(def_val);
        if (*ret_string == NULL)
            return ENOMEM;
    } else
        *ret_string = NULL;
    return 0;
}

static errcode_t
parse_int(const char *value, int *ret_int)
{
    char            *end_value;
    long            ret_long;

    if (value[0] == 0)
        /* Empty string is no good.  */
        return PROF_BAD_INTEGER;
    errno = 0;
    ret_long = strtol(value, &end_value, 10);

    /* Overflow or underflow.  */
    if ((ret_long == LONG_MIN || ret_long == LONG_MAX) && errno != 0)
        return PROF_BAD_INTEGER;
    /* Value outside "int" range.  */
    if ((long) (int) ret_long != ret_long)
        return PROF_BAD_INTEGER;
    /* Garbage in string.  */
    if (end_value != value + strlen (value))
        return PROF_BAD_INTEGER;

    *ret_int = ret_long;
    return 0;
}

errcode_t KRB5_CALLCONV
profile_get_integer(profile_t profile, const char *name, const char *subname,
                    const char *subsubname, int def_val, int *ret_int)
{
    char            *value;
    errcode_t       retval;
    const char      *names[4];

    *ret_int = def_val;
    if (profile == 0)
        return 0;

    names[0] = name;
    names[1] = subname;
    names[2] = subsubname;
    names[3] = 0;
    retval = profile_get_value(profile, names, &value);
    if (retval == PROF_NO_SECTION || retval == PROF_NO_RELATION) {
        *ret_int = def_val;
        return 0;
    } else if (retval)
        return retval;

    retval = parse_int(value, ret_int);
    free(value);
    return retval;
}

static const char *const conf_yes[] = {
    "y", "yes", "true", "t", "1", "on",
    0,
};

static const char *const conf_no[] = {
    "n", "no", "false", "nil", "0", "off",
    0,
};

static errcode_t
profile_parse_boolean(const char *s, int *ret_boolean)
{
    const char *const *p;

    if (ret_boolean == NULL)
        return PROF_EINVAL;

    for(p=conf_yes; *p; p++) {
        if (!strcasecmp(*p,s)) {
            *ret_boolean = 1;
            return 0;
        }
    }

    for(p=conf_no; *p; p++) {
        if (!strcasecmp(*p,s)) {
            *ret_boolean = 0;
            return 0;
        }
    }

    return PROF_BAD_BOOLEAN;
}

errcode_t KRB5_CALLCONV
profile_get_boolean(profile_t profile, const char *name, const char *subname,
                    const char *subsubname, int def_val, int *ret_boolean)
{
    char            *value;
    errcode_t       retval;
    const char      *names[4];

    if (profile == 0) {
        *ret_boolean = def_val;
        return 0;
    }

    names[0] = name;
    names[1] = subname;
    names[2] = subsubname;
    names[3] = 0;
    retval = profile_get_value(profile, names, &value);
    if (retval == PROF_NO_SECTION || retval == PROF_NO_RELATION) {
        *ret_boolean = def_val;
        return 0;
    } else if (retval)
        return retval;

    retval = profile_parse_boolean(value, ret_boolean);
    free(value);
    return retval;
}

/*
 * This function will return the list of the names of subections in the
 * under the specified section name.
 */
errcode_t KRB5_CALLCONV
profile_get_subsection_names(profile_t profile, const char **names,
                             char ***ret_names)
{
    errcode_t               retval;
    void                    *state;
    char                    *name;
    struct profile_string_list values;

    if ((retval = profile_iterator_create(profile, names,
                                          PROFILE_ITER_LIST_SECTION |
                                          PROFILE_ITER_SECTIONS_ONLY,
                                          &state)))
        return retval;

    if ((retval = init_list(&values)))
        return retval;

    do {
        if ((retval = profile_iterator(&state, &name, NULL)))
            goto cleanup;
        if (name)
            add_to_list(&values, name);
        free(name);
    } while (state);

    end_list(&values, ret_names);
    return 0;

cleanup:
    end_list(&values, 0);
    return retval;
}

/*
 * This function will return the list of the names of relations in the
 * under the specified section name.
 */
errcode_t KRB5_CALLCONV
profile_get_relation_names(profile_t profile, const char **names,
                           char ***ret_names)
{
    errcode_t               retval;
    void                    *state;
    char                    *name;
    struct profile_string_list values;

    if ((retval = profile_iterator_create(profile, names,
                                          PROFILE_ITER_LIST_SECTION |
                                          PROFILE_ITER_RELATIONS_ONLY,
                                          &state)))
        return retval;

    if ((retval = init_list(&values)))
        return retval;

    do {
        if ((retval = profile_iterator(&state, &name, NULL)))
            goto cleanup;
        if (name && !is_list_member(&values, name))
            add_to_list(&values, name);
        free(name);
    } while (state);

    end_list(&values, ret_names);
    return 0;

cleanup:
    end_list(&values, 0);
    return retval;
}

struct profile_iterator {
    prf_magic_t magic;
    profile_t profile;
    void *idata;
};

errcode_t KRB5_CALLCONV
profile_iterator_create(profile_t profile, const char *const *names, int flags,
                        void **ret_iter)
{
    struct profile_iterator *iter;
    errcode_t retval;

    *ret_iter = NULL;
    if (!profile)
        return PROF_NO_PROFILE;

    iter = malloc(sizeof(*iter));
    if (iter == NULL)
        return ENOMEM;
    iter->magic = PROF_MAGIC_ITERATOR;
    iter->profile = profile;

    /* Create the underlying iterator representation using the vtable or the
     * built-in node iterator. */
    if (profile->vt) {
        if (!profile->vt->iterator_create)
            retval = PROF_UNSUPPORTED;
        else
            retval = profile->vt->iterator_create(profile->cbdata, names,
                                                  flags, &iter->idata);
    } else {
        retval = profile_node_iterator_create(profile, names, flags,
                                              &iter->idata);
    }
    if (retval) {
        free(iter);
        return retval;
    }

    *ret_iter = iter;
    return 0;
}

void KRB5_CALLCONV
profile_iterator_free(void **iter_p)
{
    struct profile_iterator *iter;
    profile_t profile;

    if (!iter_p)
        return;
    iter = *iter_p;
    if (!iter || iter->magic != PROF_MAGIC_ITERATOR)
        return;
    profile = iter->profile;
    if (profile->vt)
        profile->vt->iterator_free(profile->cbdata, iter->idata);
    else
        profile_node_iterator_free(&iter->idata);
    free(iter);
    *iter_p = NULL;
}

/* Make copies of name and value into *ret_name and *ret_value.  Handle null
 * values of any argument. */
static errcode_t
set_results(const char *name, const char *value, char **ret_name,
            char **ret_value)
{
    char *name_copy = NULL, *value_copy = NULL;

    if (ret_name && name) {
        name_copy = strdup(name);
        if (name_copy == NULL)
            goto oom;
    }
    if (ret_value && value) {
        value_copy = strdup(value);
        if (value_copy == NULL)
            goto oom;
    }
    if (ret_name)
        *ret_name = name_copy;
    if (ret_value)
        *ret_value = value_copy;
    return 0;
oom:
    free(name_copy);
    free(value_copy);
    return ENOMEM;
}

errcode_t KRB5_CALLCONV
profile_iterator(void **iter_p, char **ret_name, char **ret_value)
{
    char *name, *value;
    errcode_t       retval;
    struct profile_iterator *iter = *iter_p;
    profile_t profile;

    if (ret_name)
        *ret_name = NULL;
    if (ret_value)
        *ret_value = NULL;
    if (iter == NULL || iter->magic != PROF_MAGIC_ITERATOR)
        return PROF_MAGIC_ITERATOR;
    profile = iter->profile;

    if (profile->vt) {
        retval = profile->vt->iterator(profile->cbdata, iter->idata, &name,
                                       &value);
        if (retval)
            return retval;
        if (name == NULL) {
            profile->vt->iterator_free(profile->cbdata, iter->idata);
            free(iter);
            *iter_p = NULL;
        }
        retval = set_results(name, value, ret_name, ret_value);
        if (name)
            profile->vt->free_string(profile->cbdata, name);
        if (value)
            profile->vt->free_string(profile->cbdata, value);
        return retval;
    }

    retval = profile_node_iterator(&iter->idata, 0, &name, &value);
    if (iter->idata == NULL) {
        free(iter);
        *iter_p = NULL;
    }
    if (retval)
        return retval;
    return set_results(name, value, ret_name, ret_value);
}

void KRB5_CALLCONV
profile_release_string(char *str)
{
    free(str);
}
