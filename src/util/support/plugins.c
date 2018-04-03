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
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
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

#if USE_DLOPEN && USE_CFBUNDLE
#include <CoreFoundation/CoreFoundation.h>

/* Currently CoreFoundation only exists on the Mac so we just use
 * pthreads directly to avoid creating empty function calls on other
 * platforms.  If a thread initializer ever gets created in the common
 * plugin code, move this there */
static pthread_mutex_t krb5int_bundle_mutex = PTHREAD_MUTEX_INITIALIZER;
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
#if USE_DLOPEN
    void *dlhandle;
#endif
#ifdef _WIN32
    HMODULE hinstPlugin;
#endif
#if !defined (USE_DLOPEN) && !defined (_WIN32)
    char dummy;
#endif
};

#ifdef _WIN32
struct dirent {
    long d_ino;                 /* inode (always 1 in WIN32) */
    off_t d_off;                /* offset to this dirent */
    unsigned short d_reclen;    /* length of d_name */
    char d_name[_MAX_FNAME+1];  /* filename (null terminated) */
};

typedef struct {
    intptr_t handle;            /* _findfirst/_findnext handle */
    short offset;               /* offset into directory */
    short finished;             /* 1 if there are not more files */
    struct _finddata_t fileinfo;/* from _findfirst/_findnext */
    char *dir;                  /* the dir we are reading */
    struct dirent dent;         /* the dirent to return */
} DIR;

DIR * opendir(const char *dir)
{
    DIR *dp;
    char *filespec;
    intptr_t handle;
    int index;

    filespec = malloc(strlen(dir) + 2 + 1);
    strcpy(filespec, dir);
    index = strlen(filespec) - 1;
    if (index >= 0 && (filespec[index] == '/' || filespec[index] == '\\'))
        filespec[index] = '\0';
    strcat(filespec, "/*");

    dp = (DIR *)malloc(sizeof(DIR));
    dp->offset = 0;
    dp->finished = 0;
    dp->dir = strdup(dir);

    if ((handle = _findfirst(filespec, &(dp->fileinfo))) < 0) {
        if (errno == ENOENT)
            dp->finished = 1;
        else {
            free(filespec);
            free(dp->dir);
            free(dp);
            return NULL;
        }
    }

    dp->handle = handle;
    free(filespec);

    return dp;
}

struct dirent * readdir(DIR *dp)
{
    if (!dp || dp->finished) return NULL;

    if (dp->offset != 0) {
        if (_findnext(dp->handle, &(dp->fileinfo)) < 0) {
            dp->finished = 1;
            return NULL;
        }
    }
    dp->offset++;

    strncpy(dp->dent.d_name, dp->fileinfo.name, _MAX_FNAME);
    dp->dent.d_ino = 1;
    dp->dent.d_reclen = (unsigned short)strlen(dp->dent.d_name);
    dp->dent.d_off = dp->offset;

    return &(dp->dent);
}

int closedir(DIR *dp)
{
    if (!dp) return 0;
    _findclose(dp->handle);
    free(dp->dir);
    free(dp);

    return 0;
}
#endif

long KRB5_CALLCONV
krb5int_open_plugin (const char *filepath, struct plugin_file_handle **h, struct errinfo *ep)
{
    long err = 0;
    struct stat statbuf;
    struct plugin_file_handle *htmp = NULL;
    int got_plugin = 0;

    if (!err) {
        if (stat (filepath, &statbuf) < 0) {
            err = errno;
            Tprintf ("stat(%s): %s\n", filepath, strerror (err));
            k5_set_error(ep, err, _("unable to find plugin [%s]: %s"),
                         filepath, strerror(err));
        }
    }

    if (!err) {
        htmp = calloc (1, sizeof (*htmp)); /* calloc initializes ptrs to NULL */
        if (htmp == NULL) { err = ENOMEM; }
    }

#if USE_DLOPEN
    if (!err && ((statbuf.st_mode & S_IFMT) == S_IFREG
#if USE_CFBUNDLE
                 || (statbuf.st_mode & S_IFMT) == S_IFDIR
#endif /* USE_CFBUNDLE */
        )) {
        void *handle = NULL;

#if USE_CFBUNDLE
        char executablepath[MAXPATHLEN];

        if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
            int lock_err = 0;
            CFStringRef pluginString = NULL;
            CFURLRef pluginURL = NULL;
            CFBundleRef pluginBundle = NULL;
            CFURLRef executableURL = NULL;

            /* Lock around CoreFoundation calls since objects are refcounted
             * and the refcounts are not thread-safe.  Using pthreads directly
             * because this code is Mac-specific */
            lock_err = pthread_mutex_lock(&krb5int_bundle_mutex);
            if (lock_err) { err = lock_err; }

            if (!err) {
                pluginString = CFStringCreateWithCString (kCFAllocatorDefault,
                                                          filepath,
                                                          kCFStringEncodingASCII);
                if (pluginString == NULL) { err = ENOMEM; }
            }

            if (!err) {
                pluginURL = CFURLCreateWithFileSystemPath (kCFAllocatorDefault,
                                                           pluginString,
                                                           kCFURLPOSIXPathStyle,
                                                           true);
                if (pluginURL == NULL) { err = ENOMEM; }
            }

            if (!err) {
                pluginBundle = CFBundleCreate (kCFAllocatorDefault, pluginURL);
                if (pluginBundle == NULL) { err = ENOENT; } /* XXX need better error */
            }

            if (!err) {
                executableURL = CFBundleCopyExecutableURL (pluginBundle);
                if (executableURL == NULL) { err = ENOMEM; }
            }

            if (!err) {
                if (!CFURLGetFileSystemRepresentation (executableURL,
                                                       true, /* absolute */
                                                       (UInt8 *)executablepath,
                                                       sizeof (executablepath))) {
                    err = ENOMEM;
                }
            }

            if (!err) {
                /* override the path the caller passed in */
                filepath = executablepath;
            }

            if (executableURL    != NULL) { CFRelease (executableURL); }
            if (pluginBundle     != NULL) { CFRelease (pluginBundle); }
            if (pluginURL        != NULL) { CFRelease (pluginURL); }
            if (pluginString     != NULL) { CFRelease (pluginString); }

            /* unlock after CFRelease calls since they modify refcounts */
            if (!lock_err) { pthread_mutex_unlock (&krb5int_bundle_mutex); }
        }
#endif /* USE_CFBUNDLE */

        if (!err) {
            handle = dlopen(filepath, PLUGIN_DLOPEN_FLAGS);
            if (handle == NULL) {
                const char *e = dlerror();
                if (e == NULL)
                    e = _("unknown failure");
                Tprintf ("dlopen(%s): %s\n", filepath, e);
                err = ENOENT; /* XXX */
                k5_set_error(ep, err, _("unable to load plugin [%s]: %s"),
                             filepath, e);
            }
        }

        if (!err) {
            got_plugin = 1;
            htmp->dlhandle = handle;
            handle = NULL;
        }

        if (handle != NULL) { dlclose (handle); }
    }
#endif /* USE_DLOPEN */

#ifdef _WIN32
    if (!err && (statbuf.st_mode & S_IFMT) == S_IFREG) {
        HMODULE handle = NULL;

        handle = LoadLibrary(filepath);
        if (handle == NULL) {
            Tprintf ("Unable to load dll: %s\n", filepath);
            err = ENOENT; /* XXX */
            k5_set_error(ep, err, _("unable to load DLL [%s]"), filepath);
        }

        if (!err) {
            got_plugin = 1;
            htmp->hinstPlugin = handle;
            handle = NULL;
        }

        if (handle != NULL)
            FreeLibrary(handle);
    }
#endif

    if (!err && !got_plugin) {
        err = ENOENT;  /* no plugin or no way to load plugins */
        k5_set_error(ep, err, _("plugin unavailable: %s"), strerror(err));
    }

    if (!err) {
        *h = htmp;
        htmp = NULL;  /* h takes ownership */
    }

    free(htmp);

    return err;
}

static long
krb5int_get_plugin_sym (struct plugin_file_handle *h,
                        const char *csymname, int isfunc, void **ptr,
                        struct errinfo *ep)
{
    long err = 0;
    void *sym = NULL;

#if USE_DLOPEN
    if (!err && !sym && (h->dlhandle != NULL)) {
        /* XXX Do we need to add a leading "_" to the symbol name on any
           modern platforms?  */
        sym = dlsym (h->dlhandle, csymname);
        if (sym == NULL) {
            const char *e = dlerror (); /* XXX copy and save away */
            if (e == NULL)
                e = "unknown failure";
            Tprintf ("dlsym(%s): %s\n", csymname, e);
            err = ENOENT; /* XXX */
            k5_set_error(ep, err, "%s", e);
        }
    }
#endif

#ifdef _WIN32
    LPVOID lpMsgBuf;
    DWORD dw;

    if (!err && !sym && (h->hinstPlugin != NULL)) {
        sym = GetProcAddress(h->hinstPlugin, csymname);
        if (sym == NULL) {
            const char *e = "unable to get dll symbol"; /* XXX copy and save away */
            Tprintf ("GetProcAddress(%s): %i\n", csymname, GetLastError());
            err = ENOENT; /* XXX */
            k5_set_error(ep, err, "%s", e);

            dw = GetLastError();
            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                              FORMAT_MESSAGE_FROM_SYSTEM,
                              NULL,
                              dw,
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              (LPTSTR) &lpMsgBuf,
                              0, NULL )) {

                fprintf (stderr, "unable to get dll symbol, %s\n", (LPCTSTR)lpMsgBuf);
                LocalFree(lpMsgBuf);
            }
        }
    }
#endif

    if (!err && (sym == NULL)) {
        err = ENOENT;  /* unimplemented */
    }

    if (!err) {
        *ptr = sym;
    }

    return err;
}

long KRB5_CALLCONV
krb5int_get_plugin_data (struct plugin_file_handle *h, const char *csymname,
                         void **ptr, struct errinfo *ep)
{
    return krb5int_get_plugin_sym (h, csymname, 0, ptr, ep);
}

long KRB5_CALLCONV
krb5int_get_plugin_func (struct plugin_file_handle *h, const char *csymname,
                         void (**ptr)(), struct errinfo *ep)
{
    void *dptr = NULL;
    long err = krb5int_get_plugin_sym (h, csymname, 1, &dptr, ep);
    if (!err) {
        /* Cast function pointers to avoid code duplication */
        *ptr = (void (*)()) dptr;
    }
    return err;
}

void KRB5_CALLCONV
krb5int_close_plugin (struct plugin_file_handle *h)
{
#if USE_DLOPEN
    if (h->dlhandle != NULL) { dlclose(h->dlhandle); }
#endif
#ifdef _WIN32
    if (h->hinstPlugin != NULL) { FreeLibrary(h->hinstPlugin); }
#endif
    free (h);
}

/* autoconf docs suggest using this preference order */
#if HAVE_DIRENT_H || USE_DIRENT_H
#include <dirent.h>
#define NAMELEN(D) strlen((D)->d_name)
#else
#ifndef _WIN32
#define dirent direct
#define NAMELEN(D) ((D)->d->namlen)
#else
#define NAMELEN(D) strlen((D)->d_name)
#endif
#if HAVE_SYS_NDIR_H
# include <sys/ndir.h>
#elif HAVE_SYS_DIR_H
# include <sys/dir.h>
#elif HAVE_NDIR_H
# include <ndir.h>
#endif
#endif

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
            /* load all plugins in each directory */
            DIR *dir = opendir (dirnames[i]);

            while (dir != NULL && !err) {
                struct dirent *d = NULL;
                char *filepath = NULL;
                struct plugin_file_handle *handle = NULL;

                d = readdir (dir);
                if (d == NULL) { break; }

                if ((strcmp (d->d_name, ".") == 0) ||
                    (strcmp (d->d_name, "..") == 0)) {
                    continue;
                }

                if (!err) {
                    int len = NAMELEN (d);
                    if (asprintf(&filepath, "%s/%*s", dirnames[i], len, d->d_name) < 0) {
                        filepath = NULL;
                        err = ENOMEM;
                    }
                }

                if (!err) {
                    if (krb5int_open_plugin (filepath, &handle, ep) == 0) {
                        err = krb5int_plugin_file_handle_array_add (&h, &count, handle);
                        if (!err) { handle = NULL; }  /* h takes ownership */
                    }
                }

                free(filepath);
                if (handle    != NULL) { krb5int_close_plugin (handle); }
            }

            if (dir != NULL) { closedir (dir); }
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
