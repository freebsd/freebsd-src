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
 * 18 July 1993
 *
 * Include and define various things wanted by the create command.
 *
 */

#ifndef _INST_CREATE_H_INCLUDE
#define _INST_CREATE_H_INCLUDE

extern char	*Prefix;
extern char	*Comment;
extern char	*Desc;
extern char	*Display;
extern char	*Install;
extern char	*PostInstall;
extern char	*DeInstall;
extern char	*PostDeInstall;
extern char	*Contents;
extern char	*Require;
extern char	*SrcDir;
extern char	*ExcludeFrom;
extern char	*Mtree;
extern char	*Pkgdeps;
extern char	*Origin;
extern char	*InstalledPkg;
extern char	PlayPen[];
extern int	Dereference;
extern int	PlistOnly;
extern int	UseBzip2;

void		check_list(const char *, Package *);
int		pkg_perform(char **);
void		copy_plist(const char *, Package *);

#endif	/* _INST_CREATE_H_INCLUDE */
