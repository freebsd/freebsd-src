/* $Id: add.h,v 1.3 1993/08/24 09:23:13 jkh Exp $ */

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
 * Include and define various things wanted by the add command.
 *
 */

#ifndef _INST_ADD_H_INCLUDE
#define _INST_ADD_H_INCLUDE

extern char	*Prefix;
extern Boolean	NoInstall;
extern Boolean	NoRecord;
extern char	*Mode;
extern char	*Owner;
extern char	*Group;
extern char	*Directory;
extern char	*PkgName;

int		make_hierarchy(char *);
void		extract_plist(char *, Package *);
void		apply_perms(char *, char *);

#endif	/* _INST_ADD_H_INCLUDE */
