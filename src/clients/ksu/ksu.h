/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 1994 by the University of Southern California
 *
 * EXPORT OF THIS SOFTWARE from the United States of America may
 *     require a specific license from the United States Government.
 *     It is the responsibility of any person or organization contemplating
 *     export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to copy, modify, and distribute
 *     this software and its documentation in source and binary forms is
 *     hereby granted, provided that any documentation or other materials
 *     related to such distribution or use acknowledge that the software
 *     was developed by the University of Southern California.
 *
 * DISCLAIMER OF WARRANTY.  THIS SOFTWARE IS PROVIDED "AS IS".  The
 *     University of Southern California MAKES NO REPRESENTATIONS OR
 *     WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 *     limitation, the University of Southern California MAKES NO
 *     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 *     PARTICULAR PURPOSE. The University of Southern
 *     California shall not be held liable for any liability nor for any
 *     direct, indirect, or consequential damages with respect to any
 *     claim by the user or distributor of the ksu software.
 *
 * KSU was writen by:  Ari Medvinsky, ari@isi.edu
 */

#include "k5-int.h"
#include "k5-util.h"
#include <stdio.h>
#include "com_err.h"
#include <sys/types.h>
#include <sys/param.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
/* <stdarg.h> or <varargs.h> is already included by com_err.h.  */

#define NO_TARGET_FILE '.'
#define SOURCE_USER_LOGIN "."

#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_TKT_LIFE 60*60*12 /* 12 hours */

#define KRB5_LOGIN_NAME ".k5login"
#define KRB5_USERS_NAME ".k5users"
#define USE_DEFAULT_REALM_NAME "."
#define PERMIT_ALL_COMMANDS "*"
#define KRB5_SEC_BUFFSIZE 80
#define NOT_AUTHORIZED 1

#define CHUNK 3
#define CACHE_MODE 0600
#define MAX_CMD 2048 /* this is temp, should use realloc instead,
                        as done in most of the code */


extern int optind;
extern char * optarg;

/* globals */
extern char * prog_name;
extern int auth_debug;
extern int quiet;
extern char k5login_path[MAXPATHLEN];
extern char k5users_path[MAXPATHLEN];
extern char * gb_err;
/***********/

/* krb_auth_su.c */
extern krb5_boolean krb5_auth_check
(krb5_context, krb5_principal, char *, krb5_get_init_creds_opt *,
 char *, krb5_ccache, int *, uid_t);

extern krb5_boolean krb5_fast_auth
(krb5_context, krb5_principal, krb5_principal, char *,
 krb5_ccache);

extern krb5_boolean ksu_get_tgt_via_passwd
(krb5_context,
 krb5_principal, krb5_get_init_creds_opt *, krb5_boolean *, krb5_creds *);

extern void dump_principal
(krb5_context, char *, krb5_principal);

extern void plain_dump_principal
(krb5_context, krb5_principal);


extern krb5_error_code krb5_parse_lifetime
(char *, long *);

extern krb5_error_code get_best_principal
(krb5_context, char **, krb5_principal *);

/* ccache.c */
extern krb5_error_code krb5_ccache_copy
(krb5_context, krb5_ccache, krb5_principal, krb5_ccache,
 krb5_boolean, krb5_principal, krb5_boolean *);

extern krb5_error_code krb5_store_all_creds
(krb5_context, krb5_ccache, krb5_creds **, krb5_creds **);

extern krb5_error_code krb5_store_all_creds
(krb5_context, krb5_ccache, krb5_creds **, krb5_creds **);

extern krb5_boolean compare_creds
(krb5_context, krb5_creds *, krb5_creds *);

extern krb5_error_code krb5_get_nonexp_tkts
(krb5_context, krb5_ccache, krb5_creds ***);

extern krb5_error_code krb5_check_exp
(krb5_context, krb5_ticket_times);

extern char *flags_string (krb5_creds *);

extern krb5_error_code krb5_get_login_princ
(const char *, char ***);

extern void show_credential
(krb5_context, krb5_creds *, krb5_ccache);

krb5_error_code gen_sym(krb5_context context, char **sym);

extern krb5_error_code krb5_ccache_overwrite
(krb5_context, krb5_ccache, krb5_ccache, krb5_principal);

extern krb5_error_code krb5_store_some_creds
(krb5_context, krb5_ccache, krb5_creds **, krb5_creds **,
 krb5_principal, krb5_boolean *);

extern krb5_boolean ks_ccache_name_is_initialized
(krb5_context, const char *);

extern krb5_boolean ks_ccache_is_initialized
(krb5_context, krb5_ccache);

extern krb5_error_code krb5_ccache_refresh
(krb5_context, krb5_ccache);

extern krb5_error_code krb5_ccache_filter
(krb5_context, krb5_ccache, krb5_principal);

extern krb5_boolean krb5_find_princ_in_cred_list
(krb5_context, krb5_creds **, krb5_principal);

extern krb5_error_code krb5_find_princ_in_cache
(krb5_context, krb5_ccache, krb5_principal, krb5_boolean *);

extern void printtime (krb5_timestamp);

/* authorization.c */
extern krb5_boolean fowner (FILE *, uid_t);

extern krb5_error_code krb5_authorization
(krb5_context, krb5_principal, const char *, char *,
 krb5_boolean *, char **);

extern krb5_error_code k5login_lookup (FILE *, char *,
                                       krb5_boolean *);

extern krb5_error_code k5users_lookup
(FILE *, char *, char *, krb5_boolean *, char **);

extern krb5_boolean fcmd_resolve
(char *, char ***, char **);

extern krb5_boolean cmd_single (char *);

extern int cmd_arr_cmp_postfix (char **, char *);

extern int cmd_arr_cmp (char **, char *);

extern krb5_boolean find_first_cmd_that_exists
(char **, char **, char **);

extern int match_commands
(char *, char *, krb5_boolean *, char **, char **);

extern krb5_error_code get_line (FILE *, char **);

extern char *  get_first_token (char *, char **);

extern char *  get_next_token (char **);

extern void init_auth_names (char *);

/* main.c */
extern void usage (void);

extern int standard_shell (char *);

extern krb5_error_code get_params (int *, int, char **, char ***);

/* heuristic.c */
extern krb5_error_code get_all_princ_from_file (FILE *, char ***);

extern krb5_error_code list_union (char **, char **, char ***);

extern krb5_error_code filter (FILE *, char *, char **, char ***);

extern krb5_error_code get_authorized_princ_names
(const char *, char *, char ***);

extern krb5_error_code get_closest_principal
(krb5_context, char **, krb5_principal *, krb5_boolean *);

extern krb5_error_code find_either_ticket
(krb5_context, krb5_ccache, krb5_principal,
 krb5_principal, krb5_boolean *);

extern krb5_error_code find_ticket
(krb5_context, krb5_ccache, krb5_principal,
 krb5_principal, krb5_boolean *);


extern krb5_error_code find_princ_in_list
(krb5_context, krb5_principal, char **, krb5_boolean *);

extern krb5_error_code get_best_princ_for_target
(krb5_context, uid_t, uid_t, char *, char *, krb5_ccache,
 krb5_get_init_creds_opt *, char *, char *, krb5_principal *, int *);

extern krb5_error_code ksu_tgtname (krb5_context, const krb5_data *,
                                    const krb5_data *,
                                    krb5_principal *tgtprinc);

#ifndef min
#define min(a,b) ((a) > (b) ? (b) : (a))
#endif /* min */


extern char *krb5_lname_file;  /* Note: print this out just be sure
                                  that it gets set */

extern void *xmalloc (size_t),
    *xrealloc (void *, size_t),
    *xcalloc (size_t, size_t);
                             extern char *xstrdup (const char *);
                             extern char *xasprintf (const char *format, ...);

#ifndef HAVE_UNSETENV
                             void unsetenv (char *);
#endif
