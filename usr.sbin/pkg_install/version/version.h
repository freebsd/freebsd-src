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
 * Jeremy D. Lea.
 * 11 May 2002
 *
 * This is the version module. Based on pkg_version.pl by Bruce A. Mah.
 *
 */

#ifndef _INST_VERSION_H_INCLUDE
#define _INST_VERSION_H_INCLUDE

/* Where the ports lives by default */
#define DEF_PORTS_DIR	"/usr/ports"
/* just in case we change the environment variable name */
#define PORTSDIR	"PORTSDIR"
/* macro to get name of directory where we put logging information */
#define PORTS_DIR	(getenv(PORTSDIR) ? getenv(PORTSDIR) : DEF_PORTS_DIR)

struct index_entry {
    SLIST_ENTRY(index_entry) next;
    char *name;
    char *origin;
};
SLIST_HEAD(index_head, index_entry);

extern char	*LimitChars;
extern char	*PreventChars;
extern char	*MatchName;
extern Boolean	RegexExtended;
extern Boolean	UseINDEXOnly;

extern int	version_match(char *, const char *);

#endif	/* _INST_VERSION_H_INCLUDE */
