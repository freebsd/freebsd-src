/* $FreeBSD$ */

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 23 August 1993
 *
 * Include and define various things wanted by the info command.
 *
 */

#ifndef _INST_INFO_H_INCLUDE
#define _INST_INFO_H_INCLUDE

#include <sys/queue.h>

#ifndef MAXINDEXSIZE
#define MAXINDEXSIZE 59
#endif

#ifndef MAXNAMESIZE
#define MAXNAMESIZE  20
#endif

#define	SHOW_COMMENT	0x0001
#define SHOW_DESC	0x0002
#define SHOW_PLIST	0x0004
#define SHOW_INSTALL	0x0008
#define SHOW_DEINSTALL	0x0010
#define SHOW_REQUIRE	0x0020
#define SHOW_PREFIX	0x0040
#define SHOW_INDEX	0x0080
#define SHOW_FILES	0x0100
#define SHOW_DISPLAY	0x0200
#define SHOW_REQBY	0x0400
#define SHOW_MTREE	0x0800
#define SHOW_SIZE	0x1000
#define SHOW_ORIGIN	0x2000
#define SHOW_CKSUM	0x4000
#define SHOW_FMTREV	0x8000

struct which_entry {
    TAILQ_ENTRY(which_entry) next;
    char file[PATH_MAX];
    char package[PATH_MAX];
    Boolean skip;
};
TAILQ_HEAD(which_head, which_entry);

extern int Flags;
extern Boolean Quiet;
extern char *InfoPrefix;
extern char PlayPen[];
extern char *CheckPkg;
extern match_t MatchType;
extern struct which_head *whead;

extern void	show_file(const char *, const char *);
extern void	show_plist(const char *, Package *, plist_t, Boolean);
extern void	show_files(const char *, Package *);
extern void	show_index(const char *, const char *);
extern void	show_size(const char *, Package *);
extern void	show_cksum(const char *, Package *);
extern void	show_origin(const char *, Package *);
extern void	show_fmtrev(const char *, Package *);

#endif	/* _INST_INFO_H_INCLUDE */
