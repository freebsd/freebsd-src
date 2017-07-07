/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/shlib/t_loader.c */
/*
 * Copyright (C) 2005 by the Massachusetts Institute of Technology.
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

#include "k5-platform.h"
#include "krb5.h"
#include "gssapi/gssapi.h"
#define HAVE_DLOPEN 1

static int verbose = 1;

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
#endif
/* Solaris man page recommends link.h too */

/* lazy = 1 means resolve symbols later, 0 means now; any
   other flags we should be testing?  On Windows, maybe?

   Return value is the library handle.  On error, print a message and
   exit.  */
#define do_open(LIB,REV,FLAGS) do_open_1(LIB,REV,FLAGS,__LINE__)
static void *do_open_1(const char *libname, const char *rev, int lazy, int line);

/* Look up a function symbol in the library and return a pointer.

   The return value may need casting to the correct type.  On error,
   print a message and exit.  */
static void *get_sym_1(void *libhandle, const char *sym, int line);
#define get_sym(LIB, NAME) get_sym_1(LIB, NAME, __LINE__)
#define GET_FSYM(TYPE, LIB, NAME) ((TYPE) get_sym(LIB, NAME))
#define get_gfun(LIB, NAME) ((OM_uint32 KRB5_CALLCONV(*)()) get_sym(LIB, NAME))

/* Close dynamically-opened library.

   If the OS reports an error in doing so, print a message and
   exit.  */
#define do_close(X) do_close_1(X, __LINE__)
static void do_close_1(void *libhandle, int line);

#ifdef HAVE_DLOPEN

#ifdef _AIX
# define SHLIB_SUFFIX ".a"
#else
# define SHLIB_SUFFIX ".so"
#endif

#define HORIZ 25

static void *do_open_1(const char *libname, const char *rev,
                       int lazy, int line)
{
    void *p;
    char *namebuf;
    int r;

    if (verbose)
        printf("from line %d: do_open(%s)...%*s", line, libname,
               HORIZ-strlen(libname), "");
#ifdef _AIX
    r = asprintf(&namebuf, "lib%s%s", libname, SHLIB_SUFFIX);
#else
    r = asprintf(&namebuf, "lib%s%s(shr.o.%s)", libname, SHLIB_SUFFIX, rev);
#endif
    if (r < 0) {
        perror("asprintf");
        exit(1);
    }

#ifndef RTLD_MEMBER
#define RTLD_MEMBER 0
#endif
    p = dlopen(namebuf, (lazy ? RTLD_LAZY : RTLD_NOW) | RTLD_MEMBER);
    if (p == 0) {
        fprintf(stderr, "dlopen of %s failed: %s\n", namebuf, dlerror());
        exit(1);
    }
    free(namebuf);
    if (verbose)
        printf("done: %p\n", p);
    return p;
}

#define SYM_PREFIX ""
static void *get_sym_1(void *libhandle, const char *symname, int line)
{
    void *s;

    /* Bah.  Fix this later, if we care.  */
    assert(strlen(SYM_PREFIX) == 0);

    if (verbose)
        printf("from line %d: get_sym(%s)...%*s", line, symname,
               HORIZ-strlen(symname), "");

    s = dlsym(libhandle, symname);
    if (s == 0) {
        fprintf(stderr, "symbol %s not found\n", symname);
        exit(1);
    }
    if (verbose)
        printf("done: %p\n", s);
    return s;
}

static void do_close_1(void *libhandle, int line)
{
    if (verbose) {
        char pbuf[3*sizeof(libhandle)+4];
        snprintf(pbuf, sizeof(pbuf), "%p", libhandle);
        printf("from line %d: do_close(%s)...%*s", line, pbuf,
               HORIZ-1-strlen(pbuf), "");
    }
    if (dlclose(libhandle) != 0) {
        fprintf(stderr, "dlclose failed: %s\n", dlerror());
        exit(1);
    }
    if (verbose)
        printf("done\n");
}

#elif defined _WIN32

static void *do_open(const char *libname, int lazy)
{
    /* To be written?  */
    abort();
}

static void *get_sym(void *libhandle, const char *symname)
{
    abort();
}

static void do_close(void *libhandle)
{
    abort();
}

#else

static void *do_open(const char *libname, int lazy)
{
    printf("don't know how to do dynamic loading here, punting\n");
    exit(0);
}

static void *get_sym(void *libhandle, const char *symname)
{
    abort();
}

static void do_close(void *libhandle)
{
    abort();
}

#endif

int main()
{
    void *celib, *k5lib, *gsslib, *celib2;

    (void) setvbuf(stdout, 0, _IONBF, 0);

#if 0
    /* Simplest test: Load, then unload out of order.  */
    celib = do_open("com_err", "3.0", 0);
    k5lib = do_open("krb5", "3.2", 0);
    gsslib = do_open("gssapi_krb5", "2.2", 0);
    celib2 = do_open("com_err", "3.0", 0);
    do_close(celib);
    do_close(k5lib);
    do_close(celib2);
    do_close(gsslib);
#endif

    celib = do_open("com_err", "3.0", 0);
    k5lib = do_open("krb5", "3.2", 0);
    gsslib = do_open("gssapi_krb5", "2.2", 0);
    celib2 = do_open("com_err", "3.0", 0);
    do_close(celib2);
    {
        typedef krb5_error_code KRB5_CALLCONV (*ict)(krb5_context *);
        typedef void KRB5_CALLCONV (*fct)(krb5_context);

        ict init_context = (ict) get_sym(k5lib, "krb5_init_context");
        fct free_context = (fct) get_sym(k5lib, "krb5_free_context");
        krb5_context ctx;
        krb5_error_code err;

#define CALLING(S) (verbose ? printf("at   line %d: calling %s...%*s", __LINE__, #S, (int)(HORIZ+1-strlen(#S)), "") : 0)
#define DONE() (verbose ? printf("done\n") : 0)

        CALLING(krb5_init_context);
        err = init_context(&ctx);
        DONE();
        if (err) {
            fprintf(stderr, "error 0x%lx initializing context\n",
                    (unsigned long) err);
            exit(1);
        }
        CALLING(krb5_free_context);
        free_context(ctx);
        DONE();
    }
    celib2 = do_open("com_err", "3.0", 0);
    do_close(celib);
    do_close(k5lib);
    do_close(celib2);
    do_close(gsslib);

    /* Test gssapi_krb5 without having loaded anything else.  */
    gsslib = do_open("gssapi_krb5", "2.2", 1);
    {
        OM_uint32 KRB5_CALLCONV (*init_sec_context)(OM_uint32 *, gss_cred_id_t,
                                                    gss_ctx_id_t *, gss_name_t,
                                                    gss_OID,
                                                    OM_uint32, OM_uint32,
                                                    gss_channel_bindings_t,
                                                    gss_buffer_t, gss_OID *,
                                                    gss_buffer_t,
                                                    OM_uint32 *, OM_uint32 *)
            = get_gfun(gsslib, "gss_init_sec_context");
        OM_uint32 KRB5_CALLCONV (*import_name)(OM_uint32 *, gss_buffer_t,
                                               gss_OID, gss_name_t *)
            = get_gfun(gsslib, "gss_import_name");
        OM_uint32 KRB5_CALLCONV (*release_buffer)(OM_uint32 *, gss_buffer_t)
            = get_gfun(gsslib, "gss_release_buffer");
        OM_uint32 KRB5_CALLCONV (*release_name)(OM_uint32 *, gss_name_t *)
            = get_gfun(gsslib, "gss_release_name");
        OM_uint32 KRB5_CALLCONV (*delete_sec_context)(OM_uint32 *,
                                                      gss_ctx_id_t *,
                                                      gss_buffer_t)
            = get_gfun(gsslib, "gss_delete_sec_context");

        OM_uint32 gmaj, gmin;
        OM_uint32 retflags;
        gss_ctx_id_t gctx = GSS_C_NO_CONTEXT;
        gss_buffer_desc token;
        gss_name_t target;
        static gss_buffer_desc target_name_buf = {
            9, "x@mit.edu"
        };
        static gss_OID_desc service_name = {
            10, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04"
        };

        CALLING(gss_import_name);
        gmaj = import_name(&gmin, &target_name_buf, &service_name, &target);
        DONE();
        if (gmaj != GSS_S_COMPLETE) {
            fprintf(stderr,
                    "import_name reports error major 0x%lx minor 0x%lx(%ld)\n",
                    (unsigned long) gmaj, (unsigned long) gmin,
                    (signed long) gmin);
            exit(1);
        }
        /* This will probably get different errors, depending on
           whether we have tickets at the time.  Doesn't matter much,
           we're ignoring the error and testing whether we're doing
           cleanup properly.  (Though the internal cleanup needed in
           the two cases might be different.)  */
        CALLING(gss_init_sec_context);
        gmaj = init_sec_context(&gmin, GSS_C_NO_CREDENTIAL, &gctx, target,
                                GSS_C_NULL_OID, 0, 0, NULL, GSS_C_NO_BUFFER,
                                NULL, &token, &retflags, NULL);
        DONE();
        /* Ignore success/failure indication.  */
        if (token.length) {
            CALLING(gss_release_buffer);
            release_buffer(&gmin, &token);
            DONE();
        }
        CALLING(gss_release_name);
        release_name(&gmin, &target);
        DONE();
        if (gctx != GSS_C_NO_CONTEXT) {
            CALLING(gss_delete_sec_context);
            delete_sec_context(&gmin, gctx, GSS_C_NO_BUFFER);
            DONE();
        }
    }
    do_close(gsslib);

    /* Test gssapi_krb5 with com_err already loaded, then unload
       com_err first.  */
    celib = do_open("com_err", "3.0", 1);
    gsslib = do_open("gssapi_krb5", "2.2", 1);
    {
        OM_uint32 KRB5_CALLCONV (*init_sec_context)(OM_uint32 *, gss_cred_id_t,
                                                    gss_ctx_id_t *, gss_name_t,
                                                    gss_OID,
                                                    OM_uint32, OM_uint32,
                                                    gss_channel_bindings_t,
                                                    gss_buffer_t, gss_OID *,
                                                    gss_buffer_t,
                                                    OM_uint32 *, OM_uint32 *)
            = get_gfun(gsslib, "gss_init_sec_context");
        OM_uint32 KRB5_CALLCONV (*import_name)(OM_uint32 *, gss_buffer_t,
                                               gss_OID, gss_name_t *)
            = get_gfun(gsslib, "gss_import_name");
        OM_uint32 KRB5_CALLCONV (*release_buffer)(OM_uint32 *, gss_buffer_t)
            = get_gfun(gsslib, "gss_release_buffer");
        OM_uint32 KRB5_CALLCONV (*release_name)(OM_uint32 *, gss_name_t *)
            = get_gfun(gsslib, "gss_release_name");
        OM_uint32 KRB5_CALLCONV (*delete_sec_context)(OM_uint32 *,
                                                      gss_ctx_id_t *,
                                                      gss_buffer_t)
            = get_gfun(gsslib, "gss_delete_sec_context");

        OM_uint32 gmaj, gmin;
        OM_uint32 retflags;
        gss_ctx_id_t gctx = GSS_C_NO_CONTEXT;
        gss_buffer_desc token;
        gss_name_t target;
        static gss_buffer_desc target_name_buf = {
            9, "x@mit.edu"
        };
        static gss_OID_desc service_name = {
            10, "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04"
        };

        CALLING(gss_import_name);
        gmaj = import_name(&gmin, &target_name_buf, &service_name, &target);
        DONE();
        if (gmaj != GSS_S_COMPLETE) {
            fprintf(stderr,
                    "import_name reports error major 0x%lx minor 0x%lx(%ld)\n",
                    (unsigned long) gmaj, (unsigned long) gmin,
                    (signed long) gmin);
            exit(1);
        }
        /* This will probably get different errors, depending on
           whether we have tickets at the time.  Doesn't matter much,
           we're ignoring the error and testing whether we're doing
           cleanup properly.  (Though the internal cleanup needed in
           the two cases might be different.)  */
        CALLING(gss_init_sec_context);
        gmaj = init_sec_context(&gmin, GSS_C_NO_CREDENTIAL, &gctx, target,
                                GSS_C_NULL_OID, 0, 0, NULL, GSS_C_NO_BUFFER,
                                NULL, &token, &retflags, NULL);
        DONE();
        /* Ignore success/failure indication.  */
        if (token.length) {
            CALLING(gss_release_buffer);
            release_buffer(&gmin, &token);
            DONE();
        }
        CALLING(gss_release_name);
        release_name(&gmin, &target);
        DONE();
        if (gctx != GSS_C_NO_CONTEXT) {
            CALLING(gss_delete_sec_context);
            delete_sec_context(&gmin, gctx, GSS_C_NO_BUFFER);
            DONE();
        }
    }
    do_close(celib);
    do_close(gsslib);

    return 0;
}
