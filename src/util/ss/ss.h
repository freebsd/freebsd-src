/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see mit-sipb-copyright.h.
 */

#ifndef _ss_h
#define _ss_h __FILE__

#include <errno.h>
#include <ss/ss_err.h>

#ifdef __STDC__
#define __SS_CONST const
#define __SS_PROTO (int, const char * const *, int, void *)
#else
#define __SS_CONST
#define __SS_PROTO ()
#endif

typedef __SS_CONST struct _ss_request_entry {
    __SS_CONST char * __SS_CONST *command_names; /* whatever */
    void (* __SS_CONST function) __SS_PROTO; /* foo */
    __SS_CONST char * __SS_CONST info_string;   /* NULL */
    int flags;                  /* 0 */
} ss_request_entry;

typedef __SS_CONST struct _ss_request_table {
    int version;
    ss_request_entry *requests;
} ss_request_table;

#define SS_RQT_TBL_V2   2

typedef struct _ss_rp_options { /* DEFAULT VALUES */
    int version;                /* SS_RP_V1 */
    void (*unknown) __SS_PROTO; /* call for unknown command */
    int allow_suspend;
    int catch_int;
} ss_rp_options;

#define SS_RP_V1 1

#define SS_OPT_DONT_LIST        0x0001
#define SS_OPT_DONT_SUMMARIZE   0x0002

void ss_help __SS_PROTO;
void ss_list_requests __SS_PROTO;
void ss_quit __SS_PROTO;
char *ss_current_request();
char *ss_name(int);
void ss_error (int, long, char const *, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 3, 4)))
#endif
    ;
void ss_perror (int, long, char const *);
int ss_listen (int);
int ss_create_invocation(char *, char *, char *, ss_request_table *, int *);
void ss_delete_invocation(int);
void ss_add_info_dir(int , char *, int *);
void ss_delete_info_dir(int , char *, int *);
int ss_execute_command(int sci_idx, char **);
void ss_abort_subsystem(int, int);
void ss_set_prompt(int, char *);
char *ss_get_prompt(int);
void ss_add_request_table(int, ss_request_table *, int, int *);
void ss_delete_request_table(int, ss_request_table *, int *);
int ss_execute_line (int, char*);
extern ss_request_table ss_std_requests;
#endif /* _ss_h */
