/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * prof_init.c --- routines that manipulate the user-visible profile_t
 *      object.
 */

#include "prof_int.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>

/* Create a vtable profile, possibly with a library handle.  The new profile
 * takes ownership of the handle refcount on success. */
static errcode_t
init_module(struct profile_vtable *vtable, void *cbdata,
            prf_lib_handle_t handle, profile_t *ret_profile)
{
    profile_t profile;
    struct profile_vtable *vt_copy;

    /* Check that the vtable's minor version is sane and that mandatory methods
     * are implemented. */
    if (vtable->minor_ver < 1 || !vtable->get_values || !vtable->free_values)
        return EINVAL;
    if (vtable->cleanup && !vtable->copy)
        return EINVAL;
    if (vtable->iterator_create &&
        (!vtable->iterator || !vtable->iterator_free || !vtable->free_string))
        return EINVAL;

    profile = malloc(sizeof(*profile));
    if (!profile)
        return ENOMEM;
    memset(profile, 0, sizeof(*profile));

    vt_copy = malloc(sizeof(*vt_copy));
    if (!vt_copy) {
        free(profile);
        return ENOMEM;
    }
    /* It's safe to just copy the caller's vtable for now.  If the minor
     * version is bumped, we'll need to copy individual fields. */
    *vt_copy = *vtable;

    profile->vt = vt_copy;
    profile->cbdata = cbdata;
    profile->lib_handle = handle;
    profile->magic = PROF_MAGIC_PROFILE;
    *ret_profile = profile;
    return 0;
}

/* Parse modspec into the module path and residual string. */
static errcode_t
parse_modspec(const char *modspec, char **ret_path, char **ret_residual)
{
    const char *p;
    char *path, *fullpath, *residual;
    errcode_t ret;

    *ret_path = *ret_residual = NULL;

    /* Find the separator, skipping a Windows drive letter if present. */
    p = (*modspec != '\0' && modspec[1] == ':') ? modspec + 2 : modspec;
    p = strchr(p, ':');
    if (p == NULL)
        return PROF_MODULE_SYNTAX;

    /* Copy the path. */
    path = malloc(p - modspec + 1);
    if (path == NULL)
        return ENOMEM;
    memcpy(path, modspec, p - modspec);
    path[p - modspec] = '\0';

    /* Compose the path with LIBDIR if it's not absolute. */
    ret = k5_path_join(LIBDIR, path, &fullpath);
    free(path);
    if (ret)
        return ret;

    residual = strdup(p + 1);
    if (residual == NULL) {
        free(fullpath);
        return ENOMEM;
    }

    *ret_path = fullpath;
    *ret_residual = residual;
    return 0;
}

/* Load a dynamic profile module as specified by modspec and create a vtable
 * profile for it in *ret_profile. */
static errcode_t
init_load_module(const char *modspec, profile_t *ret_profile)
{
    char *modpath = NULL, *residual = NULL;
    struct errinfo einfo = { 0 };
    prf_lib_handle_t lib_handle = NULL;
    struct plugin_file_handle *plhandle = NULL;
    void *cbdata = NULL, (*fptr)();
    int have_lock = 0, have_cbdata = 0;
    struct profile_vtable vtable = { 1 };  /* Set minor_ver to 1, rest null. */
    errcode_t err;
    profile_module_init_fn initfn;

    err = parse_modspec(modspec, &modpath, &residual);
    if (err)
        goto cleanup;

    /* Allocate a reference-counted library handle container. */
    lib_handle = malloc(sizeof(*lib_handle));
    if (lib_handle == NULL)
        goto cleanup;
    err = k5_mutex_init(&lib_handle->lock);
    if (err)
        goto cleanup;
    have_lock = 1;

    /* Open the module and get its initializer. */
    err = krb5int_open_plugin(modpath, &plhandle, &einfo);
    if (err)
        goto cleanup;
    err = krb5int_get_plugin_func(plhandle, "profile_module_init", &fptr,
                                  &einfo);
    if (err == ENOENT)
        err = PROF_MODULE_INVALID;
    if (err)
        goto cleanup;

    /* Get the profile vtable and callback data pointer. */
    initfn = (profile_module_init_fn)fptr;
    err = (*initfn)(residual, &vtable, &cbdata);
    if (err)
        goto cleanup;
    have_cbdata = 1;

    /* Create a vtable profile with the information obtained. */
    lib_handle->plugin_handle = plhandle;
    lib_handle->refcount = 1;
    err = init_module(&vtable, cbdata, lib_handle, ret_profile);

cleanup:
    free(modpath);
    free(residual);
    k5_clear_error(&einfo);
    if (err) {
        if (have_cbdata && vtable.cleanup)
            vtable.cleanup(cbdata);
        if (have_lock)
            k5_mutex_destroy(&lib_handle->lock);
        free(lib_handle);
        if (plhandle)
            krb5int_close_plugin(plhandle);
    }
    return err;
}

errcode_t KRB5_CALLCONV
profile_init_flags(const_profile_filespec_t *files, int flags,
                   profile_t *ret_profile)
{
    const_profile_filespec_t *fs;
    profile_t profile;
    prf_file_t  new_file, last = 0;
    errcode_t retval = 0, access_retval = 0;
    char *modspec = NULL, **modspec_arg;

    profile = malloc(sizeof(struct _profile_t));
    if (!profile)
        return ENOMEM;
    memset(profile, 0, sizeof(struct _profile_t));
    profile->magic = PROF_MAGIC_PROFILE;

    /*
     * If the filenames list is not specified or empty, return an empty
     * profile.
     */
    if ( files && !PROFILE_LAST_FILESPEC(*files) ) {
        for (fs = files; !PROFILE_LAST_FILESPEC(*fs); fs++) {
            /* Allow a module declaration if it is permitted by flags and this
             * is the first file parsed. */
            modspec_arg = ((flags & PROFILE_INIT_ALLOW_MODULE) && !last) ?
                &modspec : NULL;
            retval = profile_open_file(*fs, &new_file, modspec_arg);
            if (retval == PROF_MODULE && modspec) {
                /* Stop parsing files and load a dynamic module instead. */
                free(profile);
                retval = init_load_module(modspec, ret_profile);
                free(modspec);
                return retval;
            }
            /* if this file is missing, skip to the next */
            if (retval == ENOENT) {
                continue;
            }
            /* If we can't read this file, remember it but keep going. */
            if (retval == EACCES || retval == EPERM) {
                access_retval = retval;
                continue;
            }
            if (retval) {
                profile_release(profile);
                return retval;
            }
            if (last)
                last->next = new_file;
            else
                profile->first_file = new_file;
            last = new_file;
        }
        /*
         * If last is still null after the loop, then all the files were
         * missing or unreadable, so return the appropriate error.
         */
        if (!last) {
            profile_release(profile);
            return access_retval ? access_retval : ENOENT;
        }
    }

    *ret_profile = profile;
    return 0;
}

errcode_t KRB5_CALLCONV
profile_init(const_profile_filespec_t *files, profile_t *ret_profile)
{
    return profile_init_flags(files, 0, ret_profile);
}

errcode_t KRB5_CALLCONV
profile_init_vtable(struct profile_vtable *vtable, void *cbdata,
                    profile_t *ret_profile)
{
    return init_module(vtable, cbdata, NULL, ret_profile);
}

/* Copy a vtable profile. */
static errcode_t
copy_vtable_profile(profile_t profile, profile_t *ret_new_profile)
{
    errcode_t err;
    void *cbdata;
    profile_t new_profile;

    *ret_new_profile = NULL;

    if (profile->vt->copy) {
        /* Make a copy of profile's cbdata for the new profile. */
        err = profile->vt->copy(profile->cbdata, &cbdata);
        if (err)
            return err;
        err = init_module(profile->vt, cbdata, profile->lib_handle,
                          &new_profile);
        if (err && profile->vt->cleanup)
            profile->vt->cleanup(cbdata);
    } else {
        /* Use the same cbdata as the old profile. */
        err = init_module(profile->vt, profile->cbdata, profile->lib_handle,
                          &new_profile);
    }
    if (err)
        return err;

    /* Increment the refcount on the library handle if there is one. */
    if (profile->lib_handle) {
        k5_mutex_lock(&profile->lib_handle->lock);
        profile->lib_handle->refcount++;
        k5_mutex_unlock(&profile->lib_handle->lock);
    }

    *ret_new_profile = new_profile;
    return 0;
}

#define COUNT_LINKED_LIST(COUNT, PTYPE, START, FIELD)   \
    {                                                   \
        size_t cll_counter = 0;                         \
        PTYPE cll_ptr = (START);                        \
        while (cll_ptr != NULL) {                       \
            cll_counter++;                              \
            cll_ptr = cll_ptr->FIELD;                   \
        }                                               \
        (COUNT) = cll_counter;                          \
    }

errcode_t KRB5_CALLCONV
profile_copy(profile_t old_profile, profile_t *new_profile)
{
    size_t size, i;
    const_profile_filespec_t *files;
    prf_file_t file;
    errcode_t err;

    if (old_profile->vt)
        return copy_vtable_profile(old_profile, new_profile);

    /* The fields we care about are read-only after creation, so
       no locking is needed.  */
    COUNT_LINKED_LIST (size, prf_file_t, old_profile->first_file, next);
    files = malloc ((size+1) * sizeof(*files));
    if (files == NULL)
        return ENOMEM;
    for (i = 0, file = old_profile->first_file; i < size; i++, file = file->next)
        files[i] = file->data->filespec;
    files[size] = NULL;
    err = profile_init (files, new_profile);
    free (files);
    return err;
}

errcode_t KRB5_CALLCONV
profile_init_path(const_profile_filespec_list_t filepath,
                  profile_t *ret_profile)
{
    unsigned int n_entries;
    int i;
    unsigned int ent_len;
    const char *s, *t;
    profile_filespec_t *filenames;
    errcode_t retval;

    /* count the distinct filename components */
    for(s = filepath, n_entries = 1; *s; s++) {
        if (*s == ':')
            n_entries++;
    }

    /* the array is NULL terminated */
    filenames = (profile_filespec_t*) malloc((n_entries+1) * sizeof(char*));
    if (filenames == 0)
        return ENOMEM;

    /* measure, copy, and skip each one */
    for(s = filepath, i=0; (t = strchr(s, ':')) || (t=s+strlen(s)); s=t+1, i++) {
        ent_len = (unsigned int) (t-s);
        filenames[i] = (char*) malloc(ent_len + 1);
        if (filenames[i] == 0) {
            /* if malloc fails, free the ones that worked */
            while(--i >= 0) free(filenames[i]);
            free(filenames);
            return ENOMEM;
        }
        strncpy(filenames[i], s, ent_len);
        filenames[i][ent_len] = 0;
        if (*t == 0) {
            i++;
            break;
        }
    }
    /* cap the array */
    filenames[i] = 0;

    retval = profile_init_flags((const_profile_filespec_t *) filenames, 0,
                                ret_profile);

    /* count back down and free the entries */
    while(--i >= 0) free(filenames[i]);
    free(filenames);

    return retval;
}

errcode_t KRB5_CALLCONV
profile_is_writable(profile_t profile, int *writable)
{
    if (!profile || profile->magic != PROF_MAGIC_PROFILE)
        return PROF_MAGIC_PROFILE;

    if (!writable)
        return EINVAL;
    *writable = 0;

    if (profile->vt) {
        if (profile->vt->writable)
            return profile->vt->writable(profile->cbdata, writable);
        else
            return 0;
    }

    if (profile->first_file)
        *writable = profile_file_is_writable(profile->first_file);

    return 0;
}

errcode_t KRB5_CALLCONV
profile_is_modified(profile_t profile, int *modified)
{
    if (!profile || profile->magic != PROF_MAGIC_PROFILE)
        return PROF_MAGIC_PROFILE;

    if (!modified)
        return EINVAL;
    *modified = 0;

    if (profile->vt) {
        if (profile->vt->modified)
            return profile->vt->modified(profile->cbdata, modified);
        else
            return 0;
    }

    if (profile->first_file)
        *modified = (profile->first_file->data->flags & PROFILE_FILE_DIRTY);

    return 0;
}

errcode_t KRB5_CALLCONV
profile_flush(profile_t profile)
{
    if (!profile || profile->magic != PROF_MAGIC_PROFILE)
        return PROF_MAGIC_PROFILE;

    if (profile->vt) {
        if (profile->vt->flush)
            return profile->vt->flush(profile->cbdata);
        return 0;
    }

    if (profile->first_file)
        return profile_flush_file(profile->first_file);

    return 0;
}

errcode_t KRB5_CALLCONV
profile_flush_to_file(profile_t profile, const_profile_filespec_t outfile)
{
    if (!profile || profile->magic != PROF_MAGIC_PROFILE)
        return PROF_MAGIC_PROFILE;

    if (profile->vt)
        return PROF_UNSUPPORTED;

    if (profile->first_file)
        return profile_flush_file_to_file(profile->first_file,
                                          outfile);

    return 0;
}

errcode_t KRB5_CALLCONV
profile_flush_to_buffer(profile_t profile, char **buf)
{
    if (profile->vt)
        return PROF_UNSUPPORTED;
    return profile_flush_file_data_to_buffer(profile->first_file->data, buf);
}

void KRB5_CALLCONV
profile_free_buffer(profile_t profile, char *buf)
{
    free(buf);
}

void KRB5_CALLCONV
profile_abandon(profile_t profile)
{
    prf_file_t      p, next;

    if (!profile || profile->magic != PROF_MAGIC_PROFILE)
        return;

    if (profile->vt) {
        if (profile->vt->cleanup)
            profile->vt->cleanup(profile->cbdata);
        if (profile->lib_handle) {
            /* Decrement the refcount on the handle and maybe free it. */
            k5_mutex_lock(&profile->lib_handle->lock);
            if (--profile->lib_handle->refcount == 0) {
                krb5int_close_plugin(profile->lib_handle->plugin_handle);
                k5_mutex_unlock(&profile->lib_handle->lock);
                k5_mutex_destroy(&profile->lib_handle->lock);
                free(profile->lib_handle);
            } else
                k5_mutex_unlock(&profile->lib_handle->lock);
        }
        free(profile->vt);
    } else {
        for (p = profile->first_file; p; p = next) {
            next = p->next;
            profile_free_file(p);
        }
    }
    profile->magic = 0;
    free(profile);
}

void KRB5_CALLCONV
profile_release(profile_t profile)
{
    prf_file_t      p, next;

    if (!profile || profile->magic != PROF_MAGIC_PROFILE)
        return;

    if (profile->vt) {
        /* Flush the profile and then delegate to profile_abandon. */
        if (profile->vt->flush)
            profile->vt->flush(profile->cbdata);
        profile_abandon(profile);
        return;
    } else {
        for (p = profile->first_file; p; p = next) {
            next = p->next;
            profile_close_file(p);
        }
    }
    profile->magic = 0;
    free(profile);
}

/*
 * Here begins the profile serialization functions.
 */
errcode_t profile_ser_size(const char *unused, profile_t profile,
                           size_t *sizep)
{
    size_t      required;
    prf_file_t  pfp;

    required = 3*sizeof(int32_t);
    for (pfp = profile->first_file; pfp; pfp = pfp->next) {
        required += sizeof(int32_t);
        required += strlen(pfp->data->filespec);
    }
    *sizep += required;
    return 0;
}

static void pack_int32(int32_t oval, unsigned char **bufpp, size_t *remainp)
{
    store_32_be(oval, *bufpp);
    *bufpp += sizeof(int32_t);
    *remainp -= sizeof(int32_t);
}

errcode_t profile_ser_externalize(const char *unused, profile_t profile,
                                  unsigned char **bufpp, size_t *remainp)
{
    errcode_t           retval;
    size_t              required;
    unsigned char       *bp;
    size_t              remain;
    prf_file_t          pfp;
    int32_t             fcount, slen;

    required = 0;
    bp = *bufpp;
    remain = *remainp;
    retval = EINVAL;
    if (profile) {
        retval = ENOMEM;
        (void) profile_ser_size(unused, profile, &required);
        if (required <= remain) {
            fcount = 0;
            for (pfp = profile->first_file; pfp; pfp = pfp->next)
                fcount++;
            pack_int32(PROF_MAGIC_PROFILE, &bp, &remain);
            pack_int32(fcount, &bp, &remain);
            for (pfp = profile->first_file; pfp; pfp = pfp->next) {
                slen = (int32_t) strlen(pfp->data->filespec);
                pack_int32(slen, &bp, &remain);
                if (slen) {
                    memcpy(bp, pfp->data->filespec, (size_t) slen);
                    bp += slen;
                    remain -= (size_t) slen;
                }
            }
            pack_int32(PROF_MAGIC_PROFILE, &bp, &remain);
            retval = 0;
            *bufpp = bp;
            *remainp = remain;
        }
    }
    return(retval);
}

static int unpack_int32(int32_t *intp, unsigned char **bufpp,
                        size_t *remainp)
{
    if (*remainp >= sizeof(int32_t)) {
        *intp = load_32_be(*bufpp);
        *bufpp += sizeof(int32_t);
        *remainp -= sizeof(int32_t);
        return 0;
    }
    else
        return 1;
}

errcode_t profile_ser_internalize(const char *unused, profile_t *profilep,
                                  unsigned char **bufpp, size_t *remainp)
{
    errcode_t               retval;
    unsigned char   *bp;
    size_t          remain;
    int                     i;
    int32_t                 fcount, tmp;
    profile_filespec_t              *flist = 0;

    bp = *bufpp;
    remain = *remainp;
    fcount = 0;

    if (remain >= 12)
        (void) unpack_int32(&tmp, &bp, &remain);
    else
        tmp = 0;

    if (tmp != PROF_MAGIC_PROFILE) {
        retval = EINVAL;
        goto cleanup;
    }

    (void) unpack_int32(&fcount, &bp, &remain);
    retval = ENOMEM;

    flist = (profile_filespec_t *) malloc(sizeof(profile_filespec_t) * (size_t) (fcount + 1));
    if (!flist)
        goto cleanup;

    memset(flist, 0, sizeof(char *) * (size_t) (fcount+1));
    for (i=0; i<fcount; i++) {
        if (!unpack_int32(&tmp, &bp, &remain)) {
            flist[i] = (char *) malloc((size_t) (tmp+1));
            if (!flist[i])
                goto cleanup;
            memcpy(flist[i], bp, (size_t) tmp);
            flist[i][tmp] = '\0';
            bp += tmp;
            remain -= (size_t) tmp;
        }
    }

    if (unpack_int32(&tmp, &bp, &remain) ||
        (tmp != PROF_MAGIC_PROFILE)) {
        retval = EINVAL;
        goto cleanup;
    }

    if ((retval = profile_init((const_profile_filespec_t *) flist,
                               profilep)))
        goto cleanup;

    *bufpp = bp;
    *remainp = remain;

cleanup:
    if (flist) {
        for (i=0; i<fcount; i++) {
            if (flist[i])
                free(flist[i]);
        }
        free(flist);
    }
    return(retval);
}
