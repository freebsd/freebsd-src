/* $Id: info.h,v 1.7 1995/01/05 01:10:10 swallace Exp $ */

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

#ifndef MAXINDEXSIZE
#define MAXINDEXSIZE 60
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

extern int Flags;
extern Boolean AllInstalled;
extern Boolean Quiet;
extern char *InfoPrefix;
extern char PlayPen[];
extern char *CheckPkg;

extern void	show_file(char *, char *);
extern void	show_plist(char *, Package *, plist_t);
extern void	show_files(char *, Package *);
extern void	show_index(char *, char *);

#endif	/* _INST_INFO_H_INCLUDE */
