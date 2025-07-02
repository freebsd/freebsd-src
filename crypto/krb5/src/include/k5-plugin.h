/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * This software is being provided to you, the LICENSEE, by the
 * Massachusetts Institute of Technology (M.I.T.) under the following
 * license.  By obtaining, using and/or copying this software, you agree
 * that you have read, understood, and will comply with these terms and
 * conditions:
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify and distribute
 * this software and its documentation for any purpose and without fee or
 * royalty is hereby granted, provided that you agree to comply with the
 * following copyright notice and statements, including the disclaimer, and
 * that the same appear on ALL copies of the software and documentation,
 * including modifications that you make for internal use or for
 * distribution:
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT
 * be used in advertising or publicity pertaining to distribution of the
 * software.  Title to copyright in this software and any associated
 * documentation shall at all times remain with M.I.T., and USER agrees to
 * preserve same.
 *
 * Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 */

/* Just those definitions which are needed by util/support/plugins.c,
   which gets compiled before util/et is built, which happens before
   we can construct krb5.h, which is included by k5-int.h.

   So, no krb5 types.  */

#ifndef K5_PLUGIN_H
#define K5_PLUGIN_H

#if defined(_MSDOS) || defined(_WIN32)
#include "win-mac.h"
#endif
#include "autoconf.h"
#ifndef KRB5_CALLCONV
#define KRB5_CALLCONV
#define KRB5_CALLCONV_C
#endif

#include "k5-err.h"

/*
 * Plugins normally export fixed symbol names, but when statically
 * linking plugins, we need a different symbol name for each plugin.
 * The first argument to PLUGIN_SYMBOL_NAME acts as the
 * differentiator, and is only used for static plugin linking.
 *
 * Although this macro (and thus this header file) are used in plugins
 * whose code lies inside the krb5 tree, plugins maintained separately
 * from the krb5 tree do not need it; they can just use the fixed
 * symbol name unconditionally.
 */
#ifdef STATIC_PLUGINS
#define PLUGIN_SYMBOL_NAME(prefix, symbol) prefix ## _ ## symbol
#else
#define PLUGIN_SYMBOL_NAME(prefix, symbol) symbol
#endif

struct plugin_file_handle;      /* opaque */

struct plugin_dir_handle {
    /* This points to a NULL-terminated list of pointers to plugin_file_handle structs */
    struct plugin_file_handle **files;
};
#define PLUGIN_DIR_INIT(P) ((P)->files = NULL)
#define PLUGIN_DIR_OPEN(P) ((P)->files != NULL)

long KRB5_CALLCONV
krb5int_open_plugin (const char *, struct plugin_file_handle **, struct errinfo *);
void KRB5_CALLCONV
krb5int_close_plugin (struct plugin_file_handle *);

long KRB5_CALLCONV
krb5int_get_plugin_data (struct plugin_file_handle *, const char *, void **,
                         struct errinfo *);

long KRB5_CALLCONV
krb5int_get_plugin_func (struct plugin_file_handle *, const char *,
                         void (**)(), struct errinfo *);


long KRB5_CALLCONV
krb5int_open_plugin_dirs (const char * const *, const char * const *,
                          struct plugin_dir_handle *, struct errinfo *);
void KRB5_CALLCONV
krb5int_close_plugin_dirs (struct plugin_dir_handle *);

long KRB5_CALLCONV
krb5int_get_plugin_dir_data (struct plugin_dir_handle *, const char *,
                             void ***, struct errinfo *);
void KRB5_CALLCONV
krb5int_free_plugin_dir_data (void **);

long KRB5_CALLCONV
krb5int_get_plugin_dir_func (struct plugin_dir_handle *, const char *,
                             void (***)(void), struct errinfo *);
void KRB5_CALLCONV
krb5int_free_plugin_dir_func (void (**)(void));

#endif /* K5_PLUGIN_H */
