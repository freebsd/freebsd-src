/* $Id: info.h,v 1.4 1993/09/04 05:06:40 jkh Exp $ */

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

#define	SHOW_COMMENT	0x1
#define SHOW_DESC	0x2
#define SHOW_PLIST	0x4
#define SHOW_INSTALL	0x8
#define SHOW_DEINSTALL	0x10
#define SHOW_REQUIRE	0x20
#define SHOW_PREFIX	0x40
#define SHOW_INDEX	0x80
#define SHOW_FILES	0x100

extern int Flags;
extern Boolean AllInstalled;
extern Boolean Quiet;
extern char *InfoPrefix;
extern char *PlayPen;
extern char *CheckPkg;

extern void	show_file(char *, char *);
extern void	show_plist(char *, Package *, plist_t);
extern void	show_files(char *, Package *);

#endif	/* _INST_INFO_H_INCLUDE */
