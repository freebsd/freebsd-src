/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/dbutil/kdb5_util.h */
/*
 * Copyright 1992, 2008, 2009 by the Massachusetts Institute of Technology.
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

#include <kdb_log.h>

#define REALM_SEP       '@'
#define REALM_SEP_STR   "@"

extern char *progname;
#ifndef V4_DECLARES_STATIC
extern krb5_keyblock master_keyblock;
extern krb5_principal master_princ;
#endif
extern krb5_boolean dbactive;
extern int exit_status;
extern krb5_context util_context;
extern kadm5_config_params global_params;
extern int valid_master_key;
extern krb5_db_entry master_db;
extern char **db5util_db_args;
extern int    db5util_db_args_size;
extern krb5_kvno new_mkvno;
extern krb5_keyblock new_master_keyblock;
extern int add_db_arg(char *arg);

extern void usage(void);

extern void add_key
(char const *, char const *,
 krb5_const_principal, const krb5_keyblock *,
 krb5_kvno, krb5_keysalt *);
extern int set_dbname_help
(char *, char *);

extern char *kdb5_util_Init (int, char **);

extern int quit (void);

extern int check_for_match
(char *, int, krb5_db_entry *, int, int);

extern void parse_token
(char *, int *, int *, char *);

extern int create_db_entry (krb5_principal, krb5_db_entry *);

extern int kadm5_create_magic_princs (kadm5_config_params *params,
                                      krb5_context context);

extern int process_ov_principal (krb5_context kcontext, const char *fname,
                                 FILE *filep, krb5_boolean verbose,
                                 int *linenop);

extern void load_db (int argc, char **argv);
extern void dump_db (int argc, char **argv);
extern void kdb5_create (int argc, char **argv);
extern void kdb5_destroy (int argc, char **argv);
extern void kdb5_stash (int argc, char **argv);
extern void kdb5_add_mkey (int argc, char **argv);
extern void kdb5_use_mkey (int argc, char **argv);
extern void kdb5_list_mkeys (int argc, char **argv);
extern void kdb5_update_princ_encryption (int argc, char **argv);
extern void tabdump (int argc, char **argv);

extern krb5_error_code master_key_convert(krb5_context context,
                                          krb5_db_entry *db_entry);
extern void kdb5_purge_mkeys (int argc, char **argv);

extern int kadm5_create (kadm5_config_params *params);

extern krb5_error_code add_new_mkey(krb5_context, krb5_db_entry *,
                                    krb5_keyblock *, krb5_kvno);

extern krb5_kvno get_next_kvno(krb5_context, krb5_db_entry *);

void usage (void);
