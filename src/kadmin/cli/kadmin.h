/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/cli/kadmin.h */
/*
 * Copyright 2001 by the Massachusetts Institute of Technology.
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

/*
 *
 * Prototypes for kadmin functions called from SS library.
 */

#ifndef __KADMIN_H__
#define __KADMIN_H__

/* It would be nice if ss produced a header file we could reference */
extern void kadmin_startup(int argc, char *argv[], char **request_out,
                           char ***args_out);
extern int quit (void);
extern void kadmin_lock(int argc, char *argv[]);
extern void kadmin_unlock(int argc, char *argv[]);
extern void kadmin_delprinc(int argc, char *argv[]);
extern void kadmin_renameprinc(int argc, char *argv[]);
extern void kadmin_cpw(int argc, char *argv[]);
extern void kadmin_addprinc(int argc, char *argv[]);
extern void kadmin_modprinc(int argc, char *argv[]);
extern void kadmin_getprinc(int argc, char *argv[]);
extern void kadmin_getprincs(int argc, char *argv[]);
extern void kadmin_addpol(int argc, char *argv[]);
extern void kadmin_modpol(int argc, char *argv[]);
extern void kadmin_delpol(int argc, char *argv[]);
extern void kadmin_getpol(int argc, char *argv[]);
extern void kadmin_getpols(int argc, char *argv[]);
extern void kadmin_getprivs(int argc, char *argv[]);
extern void kadmin_keytab_add(int argc, char *argv[]);
extern void kadmin_keytab_remove(int argc, char *argv[]);
extern void kadmin_purgekeys(int argc, char *argv[]);
extern void kadmin_getstrings(int argc, char *argv[]);
extern void kadmin_setstring(int argc, char *argv[]);
extern void kadmin_delstring(int argc, char *argv[]);

#include <kdb.h>

krb5_error_code
randkey_princ(void *lhandle, krb5_principal princ, krb5_boolean keepold,
              int n_ks, krb5_key_salt_tuple *ks, krb5_keyblock **key,
              int *n_keys);

#include "autoconf.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

extern time_t get_date_rel(char *, time_t);

/* Yucky global variables */
extern krb5_context context;
extern char *whoami;
extern void *handle;

#endif /* __KADMIN_H__ */
