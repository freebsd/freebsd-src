/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#ifndef _ss_ss_internal_h
#define _ss_ss_internal_h __FILE__
#include "k5-platform.h"
#include <unistd.h>

typedef void * pointer;

#include "ss.h"

#if defined(__GNUC__)
#define LOCAL_ALLOC(x) __builtin_alloca(x)
#define LOCAL_FREE(x)
#else
#if defined(vax)
#define LOCAL_ALLOC(x) alloca(x)
#define LOCAL_FREE(x)
extern pointer alloca (unsigned);
#else
#if defined(__HIGHC__)  /* Barf! */
pragma on(alloca);
#define LOCAL_ALLOC(x) alloca(x)
#define LOCAL_FREE(x)
extern pointer alloca (unsigned);
#else
/* no alloca? */
#define LOCAL_ALLOC(x) malloc(x)
#define LOCAL_FREE(x) free(x)
#endif
#endif
#endif                          /* LOCAL_ALLOC stuff */

typedef char BOOL;

typedef struct _ss_abbrev_entry {
    char *name;                 /* abbrev name */
    char **abbrev;              /* new tokens to insert */
    unsigned int beginning_of_line : 1;
} ss_abbrev_entry;

typedef struct _ss_abbrev_list {
    int n_abbrevs;
    ss_abbrev_entry *first_abbrev;
} ss_abbrev_list;

typedef struct {
/*    char *path; */
    ss_abbrev_list abbrevs[127];
} ss_abbrev_info;

typedef struct _ss_data {       /* init values */
    /* this subsystem */
    char *subsystem_name;
    char *subsystem_version;
    /* current request info */
    int argc;
    char **argv;                /* arg list */
    char const *current_request; /* primary name */
    /* info directory for 'help' */
    char **info_dirs;
    /* to be extracted by subroutines */
    pointer info_ptr;           /* (void *) NULL */
    /* for ss_listen processing */
    char *prompt;
    ss_request_table **rqt_tables;
    ss_abbrev_info *abbrev_info;
    struct {
        unsigned int  escape_disabled : 1,
            abbrevs_disabled : 1;
    } flags;
    /* to get out */
    int abort;                  /* exit subsystem */
    int exit_status;
} ss_data;

#define CURRENT_SS_VERSION 1

#define ss_info(sci_idx)        (_ss_table[sci_idx])
#define ss_current_request(sci_idx,code_ptr)            \
    (*code_ptr=0,ss_info(sci_idx)->current_request)
void ss_unknown_function();
void ss_delete_info_dir();
char **ss_parse (int, char *, int *);
ss_abbrev_info *ss_abbrev_initialize (char *, int *);
void ss_page_stdin (void);
int ss_pager_create (void);
void ss_self_identify __SS_PROTO;
void ss_subsystem_name __SS_PROTO;
void ss_subsystem_version __SS_PROTO;
void ss_unimplemented __SS_PROTO;

extern ss_data **_ss_table;
extern char *ss_et_msgs[];

#ifndef HAVE_STDLIB_H
extern pointer malloc (unsigned);
extern pointer realloc (pointer, unsigned);
extern pointer calloc (unsigned, unsigned);
#endif

#if defined(USE_SIGPROCMASK) && !defined(POSIX_SIGNALS)
/* fake sigmask, sigblock, sigsetmask */
#include <signal.h>
#ifdef sigmask
#undef sigmask
#endif
#define sigmask(x) (1L<<(x)-1)
#define sigsetmask(x) sigprocmask(SIG_SETMASK,&x,NULL)
static int _fake_sigstore;
#define sigblock(x) (_fake_sigstore=x,sigprocmask(SIG_BLOCK,&_fake_sigstore,0))
#endif
#endif /* _ss_internal_h */
