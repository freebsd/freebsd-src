/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/plugins.c - Plugin module support functions */
/*
 * Copyright 2006, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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

#include "k5-platform.h"
#include "k5-plugin.h"
#if USE_DLOPEN
#include <dlfcn.h>
#endif

#if USE_DLOPEN
#ifdef RTLD_GROUP
#define GROUP RTLD_GROUP
#else
#define GROUP 0
#endif
#ifdef RTLD_NODELETE
#define NODELETE RTLD_NODELETE
#else
#define NODELETE 0
#endif
#define PLUGIN_DLOPEN_FLAGS (RTLD_NOW | RTLD_LOCAL | GROUP | NODELETE)
#endif

/*
 * glibc bug 11941, fixed in release 2.25, can cause an assertion failure in
 * dlclose() on process exit.  Our workaround is to leak dlopen() handles
 * (which doesn't typically manifest in leak detection tools because the
 * handles are still reachable via a global table in libdl).  Because we
 * dlopen() with RTLD_NODELETE, we weren't going to unload the plugin objects
 * anyway.
 */
#ifdef __GLIBC_PREREQ
#if ! __GLIBC_PREREQ(2, 25)
#define dlclose(x)
#endif
#endif

#include <stdarg.h>
static void Tprintf (const char *fmt, ...)
{
#ifdef DEBUG
    va_list va;
    va_start (va, fmt);
    vfprintf (stderr, fmt, va);
    va_end (va);
#endif
}

struct plugin_file_handle {
#if defined(USE_DLOPEN)
    void *dlhandle;
#elif defined(_WIN32)
    HMODULE module;
#else
    char dummy;
#endif
};

#if defined(USE_DLOPEN)

static long
open_plugin_dlfcn(struct plugin_file_handle *h, const char *filename,
                  struct errinfo *ep)
{
    const char *e;

    h->dlhandle = dlopen(filename, PLUGIN_DLOPEN_FLAGS);
    if (h->dlhandle == NULL) {
        e = dlerror();
        if (e == NULL)
            e = _("unknown failure");
        Tprintf("dlopen(%s): %s\n", filename, e);
        k5_set_error(ep, ENOENT, _("unable to load plugin [%s]: %s"),
                     filename, e);
        return ENOENT;
    }
    return 0;
}
#define open_plugin open_plugin_dlfcn

static long
get_sym_dlfcn(struct plugin_file_handle *h, const char *csymname,
              void **sym_out, struct errinfo *ep)
{
    const char *e;

    if (h->dlhandle == NULL)
        return ENOENT;
    *sym_out = dlsym(h->dlhandle, csymname);
    if (*sym_out == NULL) {
        e = dlerror();
        if (e == NULL)
            e = _("unknown failure");
        Tprintf("dlsym(%s): %s\n", csymname, e);
        k5_set_error(ep, ENOENT, "%s", e);
        return ENOENT;
    }
    return 0;
}
#define get_sym get_sym_dlfcn

static void
close_plugin_dlfcn(struct plugin_file_handle *h)
{
    if (h->dlhandle != NULL)
        dlclose(h->dlhandle);
}
#define close_plugin close_plugin_dlfcn

#elif defined(_WIN32)

static long
open_plugin_win32(struct plugin_file_handle *h, const char *filename,
                  struct errinfo *ep)
{
    h->module = LoadLibrary(filename);
    if (h == NULL) {
        Tprintf("Unable to load dll: %s\n", filename);
        k5_set_error(ep, ENOENT, _("unable to load DLL [%s]"), filename);
        return ENOENT;
    }
    return 0;
}
#define open_plugin open_plugin_win32

static long
get_sym_win32(struct plugin_file_handle *h, const char *csymname,
              void **sym_out, struct errinfo *ep)
{
    LPVOID lpMsgBuf;
    DWORD dw;

    if (h->module == NULL)
        return ENOENT;
    *sym_out = GetProcAddress(h->module, csymname);
    if (*sym_out == NULL) {
        Tprintf("GetProcAddress(%s): %i\n", csymname, GetLastError());
        dw = GetLastError();
        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM,
                          NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPTSTR)&lpMsgBuf, 0, NULL)) {
            k5_set_error(ep, ENOENT, _("unable to get DLL Symbol: %s"),
                         (char *)lpMsgBuf);
            LocalFree(lpMsgBuf);
        }
        return ENOENT;
    }
    return 0;
}
#define get_sym get_sym_win32

static void
close_plugin_win32(struct plugin_file_handle *h)
{
    if (h->module != NULL)
        FreeLibrary(h->module);
}
#define close_plugin close_plugin_win32

#else

static long
open_plugin_dummy(struct plugin_file_handle *h, const char *filename,
                  struct errinfo *ep)
{
    k5_set_error(ep, ENOENT, _("plugin loading unavailable"));
    return ENOENT;
}
#define open_plugin open_plugin_dummy

static long
get_sym_dummy(struct plugin_file_handle *h, const char *csymname,
              void **sym_out, struct errinfo *ep)
{
    return ENOENT;
}
#define get_sym get_sym_dummy

static void
close_plugin_dummy(struct plugin_file_handle *h)
{
}
#define close_plugin close_plugin_dummy

#endif

long KRB5_CALLCONV
krb5int_open_plugin(const char *filename,
                    struct plugin_file_handle **handle_out, struct errinfo *ep)
{
    long ret;
    struct plugin_file_handle *h;

    *handle_out = NULL;

    h = calloc(1, sizeof(*h));
    if (h == NULL)
        return ENOMEM;

    ret = open_plugin(h, filename, ep);
    if (ret) {
        free(h);
        return ret;
    }

    *handle_out = h;
    return 0;
}

long KRB5_CALLCONV
krb5int_get_plugin_data(struct plugin_file_handle *h, const char *csymname,
                        void **sym_out, struct errinfo *ep)
{
    return get_sym(h, csymname, sym_out, ep);
}

long KRB5_CALLCONV
krb5int_get_plugin_func(struct plugin_file_handle *h, const char *csymname,
                        void (**sym_out)(), struct errinfo *ep)
{
    void *dptr = NULL;
    long ret = get_sym(h, csymname, &dptr, ep);

    if (!ret)
        *sym_out = (void (*)())dptr;
    return ret;
}

void KRB5_CALLCONV
krb5int_close_plugin (struct plugin_file_handle *h)
{
    close_plugin(h);
    free(h);
}

static long
krb5int_plugin_file_handle_array_init (struct plugin_file_handle ***harray)
{
    long err = 0;

    *harray = calloc (1, sizeof (**harray)); /* calloc initializes to NULL */
    if (*harray == NULL) { err = ENOMEM; }

    return err;
}

static long
krb5int_plugin_file_handle_array_add (struct plugin_file_handle ***harray, size_t *count,
                                      struct plugin_file_handle *p)
{
    long err = 0;
    struct plugin_file_handle **newharray = NULL;
    size_t newcount = *count + 1;

    newharray = realloc (*harray, ((newcount + 1) * sizeof (**harray))); /* +1 for NULL */
    if (newharray == NULL) {
        err = ENOMEM;
    } else {
        newharray[newcount - 1] = p;
        newharray[newcount] = NULL;
        *count = newcount;
        *harray = newharray;
    }

    return err;
}

static void
krb5int_plugin_file_handle_array_free (struct plugin_file_handle **harray)
{
    if (harray != NULL) {
        int i;
        for (i = 0; harray[i] != NULL; i++) {
            krb5int_close_plugin (harray[i]);
        }
        free (harray);
    }
}

#if TARGET_OS_MAC
#define FILEEXTS { "", ".bundle", ".dylib", ".so", NULL }
#elif defined(_WIN32)
#define FILEEXTS  { "", ".dll", NULL }
#else
#define FILEEXTS  { "", ".so", NULL }
#endif


static void
krb5int_free_plugin_filenames (char **filenames)
{
    if (filenames != NULL) {
        int i;
        for (i = 0; filenames[i] != NULL; i++) {
            free (filenames[i]);
        }
        free (filenames);
    }
}


static long
krb5int_get_plugin_filenames (const char * const *filebases, char ***filenames)
{
    long err = 0;
    static const char *const fileexts[] = FILEEXTS;
    char **tempnames = NULL;
    size_t bases_count = 0;
    size_t exts_count = 0;
    size_t i;

    if (!filebases) { err = EINVAL; }
    if (!filenames) { err = EINVAL; }

    if (!err) {
        for (i = 0; filebases[i]; i++) { bases_count++; }
        for (i = 0; fileexts[i]; i++) { exts_count++; }
        tempnames = calloc ((bases_count * exts_count)+1, sizeof (char *));
        if (!tempnames) { err = ENOMEM; }
    }

    if (!err) {
        size_t j;
        for (i = 0; !err && filebases[i]; i++) {
            for (j = 0; !err && fileexts[j]; j++) {
                if (asprintf(&tempnames[(i*exts_count)+j], "%s%s",
                             filebases[i], fileexts[j]) < 0) {
                    tempnames[(i*exts_count)+j] = NULL;
                    err = ENOMEM;
                }
            }
        }
        tempnames[bases_count * exts_count] = NULL; /* NUL-terminate */
    }

    if (!err) {
        *filenames = tempnames;
        tempnames = NULL;
    }

    krb5int_free_plugin_filenames(tempnames);

    return err;
}


/* Takes a NULL-terminated list of directories.  If filebases is NULL, filebases is ignored
 * all plugins in the directories are loaded.  If filebases is a NULL-terminated array of names,
 * only plugins in the directories with those name (plus any platform extension) are loaded. */

long KRB5_CALLCONV
krb5int_open_plugin_dirs (const char * const *dirnames,
                          const char * const *filebases,
                          struct plugin_dir_handle *dirhandle,
                          struct errinfo *ep)
{
    long err = 0;
    struct plugin_file_handle **h = NULL;
    size_t count = 0;
    char **filenames = NULL;
    int i;

    if (!err) {
        err = krb5int_plugin_file_handle_array_init (&h);
    }

    if (!err && (filebases != NULL)) {
        err = krb5int_get_plugin_filenames (filebases, &filenames);
    }

    for (i = 0; !err && dirnames[i] != NULL; i++) {
        if (filenames != NULL) {
            /* load plugins with names from filenames from each directory */
            int j;

            for (j = 0; !err && filenames[j] != NULL; j++) {
                struct plugin_file_handle *handle = NULL;
                char *filepath = NULL;

                if (!err) {
                    if (asprintf(&filepath, "%s/%s", dirnames[i], filenames[j]) < 0) {
                        filepath = NULL;
                        err = ENOMEM;
                    }
                }

                if (!err && krb5int_open_plugin(filepath, &handle, ep) == 0) {
                    err = krb5int_plugin_file_handle_array_add (&h, &count, handle);
                    if (!err)
                        handle = NULL; /* h takes ownership */
                }

                free(filepath);
                if (handle   != NULL) { krb5int_close_plugin (handle); }
            }
        } else {
            char **fnames = NULL;
            int j;

            err = k5_dir_filenames(dirnames[i], &fnames);
            for (j = 0; !err && fnames[j] != NULL; j++) {
                char *filepath = NULL;
                struct plugin_file_handle *handle = NULL;

                if (strcmp(fnames[j], ".") == 0 ||
                    strcmp(fnames[j], "..") == 0)
                    continue;

                if (asprintf(&filepath, "%s/%s", dirnames[i], fnames[j]) < 0) {
                    filepath = NULL;
                    err = ENOMEM;
                }

                if (!err && krb5int_open_plugin(filepath, &handle, ep) == 0) {
                    err = krb5int_plugin_file_handle_array_add(&h, &count,
                                                               handle);
                    if (!err)
                        handle = NULL;  /* h takes ownership */
                }

                free(filepath);
                if (handle != NULL)
                    krb5int_close_plugin(handle);
            }

            k5_free_filenames(fnames);
        }
    }

    if (err == ENOENT) {
        err = 0;  /* ran out of plugins -- do nothing */
    }

    if (!err) {
        dirhandle->files = h;
        h = NULL;  /* dirhandle->files takes ownership */
    }

    if (filenames != NULL) { krb5int_free_plugin_filenames (filenames); }
    if (h         != NULL) { krb5int_plugin_file_handle_array_free (h); }

    return err;
}

void KRB5_CALLCONV
krb5int_close_plugin_dirs (struct plugin_dir_handle *dirhandle)
{
    if (dirhandle->files != NULL) {
        int i;
        for (i = 0; dirhandle->files[i] != NULL; i++) {
            krb5int_close_plugin (dirhandle->files[i]);
        }
        free (dirhandle->files);
        dirhandle->files = NULL;
    }
}

void KRB5_CALLCONV
krb5int_free_plugin_dir_data (void **ptrs)
{
    /* Nothing special to be done per pointer.  */
    free(ptrs);
}

long KRB5_CALLCONV
krb5int_get_plugin_dir_data (struct plugin_dir_handle *dirhandle,
                             const char *symname,
                             void ***ptrs,
                             struct errinfo *ep)
{
    long err = 0;
    void **p = NULL;
    size_t count = 0;

    /* XXX Do we need to add a leading "_" to the symbol name on any
       modern platforms?  */

    Tprintf("get_plugin_data_sym(%s)\n", symname);

    if (!err) {
        p = calloc (1, sizeof (*p)); /* calloc initializes to NULL */
        if (p == NULL) { err = ENOMEM; }
    }

    if (!err && (dirhandle != NULL) && (dirhandle->files != NULL)) {
        int i = 0;

        for (i = 0; !err && (dirhandle->files[i] != NULL); i++) {
            void *sym = NULL;

            if (krb5int_get_plugin_data (dirhandle->files[i], symname, &sym, ep) == 0) {
                void **newp = NULL;

                count++;
                newp = realloc (p, ((count + 1) * sizeof (*p))); /* +1 for NULL */
                if (newp == NULL) {
                    err = ENOMEM;
                } else {
                    p = newp;
                    p[count - 1] = sym;
                    p[count] = NULL;
                }
            }
        }
    }

    if (!err) {
        *ptrs = p;
        p = NULL; /* ptrs takes ownership */
    }

    free(p);

    return err;
}

void KRB5_CALLCONV
krb5int_free_plugin_dir_func (void (**ptrs)(void))
{
    /* Nothing special to be done per pointer.  */
    free(ptrs);
}

long KRB5_CALLCONV
krb5int_get_plugin_dir_func (struct plugin_dir_handle *dirhandle,
                             const char *symname,
                             void (***ptrs)(void),
                             struct errinfo *ep)
{
    long err = 0;
    void (**p)() = NULL;
    size_t count = 0;

    /* XXX Do we need to add a leading "_" to the symbol name on any
       modern platforms?  */

    Tprintf("get_plugin_data_sym(%s)\n", symname);

    if (!err) {
        p = calloc (1, sizeof (*p)); /* calloc initializes to NULL */
        if (p == NULL) { err = ENOMEM; }
    }

    if (!err && (dirhandle != NULL) && (dirhandle->files != NULL)) {
        int i = 0;

        for (i = 0; !err && (dirhandle->files[i] != NULL); i++) {
            void (*sym)() = NULL;

            if (krb5int_get_plugin_func (dirhandle->files[i], symname, &sym, ep) == 0) {
                void (**newp)() = NULL;

                count++;
                newp = realloc (p, ((count + 1) * sizeof (*p))); /* +1 for NULL */
                if (newp == NULL) {
                    err = ENOMEM;
                } else {
                    p = newp;
                    p[count - 1] = sym;
                    p[count] = NULL;
                }
            }
        }
    }

    if (!err) {
        *ptrs = p;
        p = NULL; /* ptrs takes ownership */
    }

    free(p);

    return err;
}
