/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * prof_set.c --- routines that expose the public interfaces for
 *      inserting, updating and deleting items from the profile.
 *
 * WARNING: These routines only look at the first file opened in the
 * profile.  It's not clear how to handle multiple files, actually.
 * In the future it may be necessary to modify this public interface,
 * or possibly add higher level functions to support this correctly.
 *
 * WARNING: We're not yet doing locking yet, either.
 *
 */

#include "prof_int.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>

static errcode_t rw_setup(profile_t profile)
{
    prf_file_t      file;
    errcode_t       retval = 0;

    if (!profile)
        return PROF_NO_PROFILE;

    if (profile->magic != PROF_MAGIC_PROFILE)
        return PROF_MAGIC_PROFILE;

    file = profile->first_file;

    profile_lock_global();

    /* Don't update the file if we've already made modifications */
    if (file->data->flags & PROFILE_FILE_DIRTY) {
        profile_unlock_global();
        return 0;
    }

    if ((file->data->flags & PROFILE_FILE_SHARED) != 0) {
        prf_data_t new_data;
        new_data = profile_make_prf_data(file->data->filespec);
        if (new_data == NULL) {
            retval = ENOMEM;
        } else {
            retval = k5_mutex_init(&new_data->lock);
            if (retval == 0) {
                new_data->root = NULL;
                new_data->flags = file->data->flags & ~PROFILE_FILE_SHARED;
                new_data->timestamp = 0;
                new_data->upd_serial = file->data->upd_serial;
            }
        }

        if (retval != 0) {
            profile_unlock_global();
            free(new_data);
            return retval;
        }
        profile_dereference_data_locked(file->data);
        file->data = new_data;
    }

    profile_unlock_global();
    retval = profile_update_file(file, NULL);

    return retval;
}


/*
 * Delete or update a particular child node
 *
 * ADL - 2/23/99, rewritten TYT 2/25/99
 */
errcode_t KRB5_CALLCONV
profile_update_relation(profile_t profile, const char **names,
                        const char *old_value, const char *new_value)
{
    errcode_t       retval;
    struct profile_node *section, *node;
    void            *state;
    const char      **cpp;

    if (profile->vt) {
        if (!profile->vt->update_relation)
            return PROF_UNSUPPORTED;
        return profile->vt->update_relation(profile->cbdata, names, old_value,
                                            new_value);
    }

    retval = rw_setup(profile);
    if (retval)
        return retval;

    if (names == 0 || names[0] == 0 || names[1] == 0)
        return PROF_BAD_NAMESET;

    if (!old_value || !*old_value)
        return PROF_EINVAL;

    k5_mutex_lock(&profile->first_file->data->lock);
    section = profile->first_file->data->root;
    for (cpp = names; cpp[1]; cpp++) {
        state = 0;
        retval = profile_find_node(section, *cpp, 0, 1,
                                   &state, &section);
        if (retval) {
            k5_mutex_unlock(&profile->first_file->data->lock);
            return retval;
        }
    }

    state = 0;
    retval = profile_find_node(section, *cpp, old_value, 0, &state, &node);
    if (retval == 0) {
        if (new_value)
            retval = profile_set_relation_value(node, new_value);
        else
            retval = profile_remove_node(node);
    }
    if (retval == 0)
        profile->first_file->data->flags |= PROFILE_FILE_DIRTY;
    k5_mutex_unlock(&profile->first_file->data->lock);

    return retval;
}

/*
 * Clear a particular all of the relations with a specific name.
 *
 * TYT - 2/25/99
 */
errcode_t KRB5_CALLCONV
profile_clear_relation(profile_t profile, const char **names)
{
    errcode_t       retval;
    struct profile_node *section, *node;
    void            *state;
    const char      **cpp;

    if (profile->vt) {
        if (!profile->vt->update_relation)
            return PROF_UNSUPPORTED;
        return profile->vt->update_relation(profile->cbdata, names, NULL,
                                            NULL);
    }

    retval = rw_setup(profile);
    if (retval)
        return retval;

    if (names == 0 || names[0] == 0 || names[1] == 0)
        return PROF_BAD_NAMESET;

    section = profile->first_file->data->root;
    for (cpp = names; cpp[1]; cpp++) {
        state = 0;
        retval = profile_find_node(section, *cpp, 0, 1,
                                   &state, &section);
        if (retval)
            return retval;
    }

    state = 0;
    do {
        retval = profile_find_node(section, *cpp, 0, 0, &state, &node);
        if (retval)
            return retval;
        retval = profile_remove_node(node);
        if (retval)
            return retval;
    } while (state);

    profile->first_file->data->flags |= PROFILE_FILE_DIRTY;

    return 0;
}

/*
 * Rename a particular section; if the new_section name is NULL,
 * delete it.
 *
 * ADL - 2/23/99, rewritten TYT 2/25/99
 */
errcode_t KRB5_CALLCONV
profile_rename_section(profile_t profile, const char **names,
                       const char *new_name)
{
    errcode_t       retval;
    struct profile_node *section, *node;
    void            *state;
    const char      **cpp;

    if (profile->vt) {
        if (!profile->vt->rename_section)
            return PROF_UNSUPPORTED;
        return profile->vt->rename_section(profile->cbdata, names, new_name);
    }

    retval = rw_setup(profile);
    if (retval)
        return retval;

    if (names == 0 || names[0] == 0)
        return PROF_BAD_NAMESET;

    k5_mutex_lock(&profile->first_file->data->lock);
    section = profile->first_file->data->root;
    for (cpp = names; cpp[1]; cpp++) {
        state = 0;
        retval = profile_find_node(section, *cpp, 0, 1,
                                   &state, &section);
        if (retval) {
            k5_mutex_unlock(&profile->first_file->data->lock);
            return retval;
        }
    }

    state = 0;
    retval = profile_find_node(section, *cpp, 0, 1, &state, &node);
    if (retval == 0) {
        if (new_name)
            retval = profile_rename_node(node, new_name);
        else
            retval = profile_remove_node(node);
    }
    if (retval == 0)
        profile->first_file->data->flags |= PROFILE_FILE_DIRTY;
    k5_mutex_unlock(&profile->first_file->data->lock);
    return retval;
}

/*
 * Insert a new relation.  If the new_value argument is NULL, then
 * create a new section instead.
 *
 * Note: if the intermediate sections do not exist, this function will
 * automatically create them.
 *
 * ADL - 2/23/99, rewritten TYT 2/25/99
 */
errcode_t KRB5_CALLCONV
profile_add_relation(profile_t profile, const char **names,
                     const char *new_value)
{
    errcode_t       retval;
    struct profile_node *section;
    const char      **cpp;
    void            *state;

    if (profile->vt) {
        if (!profile->vt->add_relation)
            return PROF_UNSUPPORTED;
        return profile->vt->add_relation(profile->cbdata, names, new_value);
    }

    retval = rw_setup(profile);
    if (retval)
        return retval;

    /* Require at least two names for a new relation, one for a new section. */
    if (names == 0 || names[0] == 0 || (names[1] == 0 && new_value))
        return PROF_BAD_NAMESET;

    k5_mutex_lock(&profile->first_file->data->lock);
    section = profile->first_file->data->root;
    for (cpp = names; cpp[1]; cpp++) {
        state = 0;
        retval = profile_find_node(section, *cpp, 0, 1,
                                   &state, &section);
        if (retval == PROF_NO_SECTION)
            retval = profile_add_node(section, *cpp, 0, &section);
        if (retval) {
            k5_mutex_unlock(&profile->first_file->data->lock);
            return retval;
        }
    }

    if (new_value == 0) {
        state = 0;
        retval = profile_find_node(section, *cpp, 0, 1, &state, 0);
        if (retval == 0) {
            k5_mutex_unlock(&profile->first_file->data->lock);
            return PROF_EXISTS;
        } else if (retval != PROF_NO_SECTION) {
            k5_mutex_unlock(&profile->first_file->data->lock);
            return retval;
        }
    }

    retval = profile_add_node(section, *cpp, new_value, 0);
    if (retval) {
        k5_mutex_unlock(&profile->first_file->data->lock);
        return retval;
    }

    profile->first_file->data->flags |= PROFILE_FILE_DIRTY;
    k5_mutex_unlock(&profile->first_file->data->lock);
    return 0;
}
