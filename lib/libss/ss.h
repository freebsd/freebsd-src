/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see mit-sipb-copyright.h.
 */

#ifndef _ss_h
#define _ss_h __FILE__

#if 0
#include <ss/mit-sipb-copyright.h>
#endif
#ifndef IN_MK_CMDS
#  ifdef IN_LIBSS
#    include "ss_err.h"
#  else
#    include <ss/ss_err.h>
#  endif
#endif

extern int errno;

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
    __SS_CONST char * __SS_CONST info_string;	/* NULL */
    int flags;			/* 0 */
} ss_request_entry;

typedef __SS_CONST struct _ss_request_table {
    int version;
    ss_request_entry *requests;
} ss_request_table;

#define SS_RQT_TBL_V2	2

typedef struct _ss_rp_options {	/* DEFAULT VALUES */
    int version;		/* SS_RP_V1 */
    void (*unknown) __SS_PROTO;	/* call for unknown command */
    int allow_suspend;
    int catch_int;
} ss_rp_options;

#define SS_RP_V1 1

#define SS_OPT_DONT_LIST	0x0001
#define SS_OPT_DONT_SUMMARIZE	0x0002

void ss_help __SS_PROTO;
char *ss_current_request();
char *ss_name();
#ifdef __STDC__
void ss_error (int, long, char const *, ...);
void ss_perror (int, long, char const *);
#else
void ss_error ();
void ss_perror ();
#endif
void ss_abort_subsystem();
extern ss_request_table ss_std_requests;
#endif /* _ss_h */
