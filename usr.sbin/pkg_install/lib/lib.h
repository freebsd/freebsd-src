/* $FreeBSD: src/usr.sbin/pkg_install/lib/lib.h,v 1.29.2.2 2000/09/20 08:53:55 jkh Exp $ */

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
 * Include and define various things wanted by the library routines.
 *
 */

#ifndef _INST_LIB_LIB_H_
#define _INST_LIB_LIB_H_

/* Includes */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Macros */
#define SUCCESS	(0)
#define	FAIL	(-1)

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#define YES		2
#define NO		1

/* Usually "rm", but often "echo" during debugging! */
#define REMOVE_CMD	"rm"

/* Usually "rm", but often "echo" during debugging! */
#define RMDIR_CMD	"rmdir"

/* Where we put logging information by default, else ${PKG_DBDIR} if set */
#define DEF_LOG_DIR		"/var/db/pkg"
/* just in case we change the environment variable name */
#define PKG_DBDIR		"PKG_DBDIR"

/* The names of our "special" files */
#define CONTENTS_FNAME		"+CONTENTS"
#define COMMENT_FNAME		"+COMMENT"
#define DESC_FNAME		"+DESC"
#define INSTALL_FNAME		"+INSTALL"
#define POST_INSTALL_FNAME	"+POST-INSTALL"
#define DEINSTALL_FNAME		"+DEINSTALL"
#define POST_DEINSTALL_FNAME	"+POST-DEINSTALL"
#define REQUIRE_FNAME		"+REQUIRE"
#define REQUIRED_BY_FNAME	"+REQUIRED_BY"
#define DISPLAY_FNAME		"+DISPLAY"
#define MTREE_FNAME		"+MTREE_DIRS"

#define CMD_CHAR		'@'	/* prefix for extended PLIST cmd */

/* The name of the "prefix" environment variable given to scripts */
#define PKG_PREFIX_VNAME	"PKG_PREFIX"

enum _plist_t {
    PLIST_FILE, PLIST_CWD, PLIST_CMD, PLIST_CHMOD,
    PLIST_CHOWN, PLIST_CHGRP, PLIST_COMMENT, PLIST_IGNORE,
    PLIST_NAME, PLIST_UNEXEC, PLIST_SRC, PLIST_DISPLAY,
    PLIST_PKGDEP, PLIST_MTREE, PLIST_DIR_RM, PLIST_IGNORE_INST,
    PLIST_OPTION
};
typedef enum _plist_t plist_t;

/* Types */
typedef unsigned int Boolean;

struct _plist {
    struct _plist *prev, *next;
    char *name;
    Boolean marked;
    plist_t type;
};
typedef struct _plist *PackingList;

struct _pack {
    struct _plist *head, *tail;
};
typedef struct _pack Package;

/* Prototypes */
/* Misc */
int		vsystem(const char *, ...);
void		cleanup(int);
char		*make_playpen(char *, size_t);
char		*where_playpen(void);
void		leave_playpen(void);
off_t		min_free(char *);

/* String */
char 		*get_dash_string(char **);
char		*copy_string(char *);
Boolean		suffix(char *, char *);
void		nuke_suffix(char *);
void		str_lowercase(char *);
char		*basename_of(char *);
char		*strconcat(char *, char *);

/* File */
Boolean		fexists(char *);
Boolean		isdir(char *);
Boolean		isemptydir(char *fname);
Boolean		isemptyfile(char *fname);
Boolean         isfile(char *);
Boolean		isempty(char *);
Boolean		issymlink(char *);
Boolean		isURL(char *);
char		*fileGetURL(char *, char *);
char		*fileFindByPath(char *, char *);
char		*fileGetContents(char *);
void		write_file(char *, char *);
void		copy_file(char *, char *, char *);
void		move_file(char *, char *, char *);
void		copy_hierarchy(char *, char *, Boolean);
int		delete_hierarchy(char *, Boolean, Boolean);
int		unpack(char *, char *);
void		format_cmd(char *, char *, char *, char *);

/* Msg */
void		upchuck(const char *);
void		barf(const char *, ...);
void		whinge(const char *, ...);
Boolean		y_or_n(Boolean, const char *, ...);

/* Packing list */
PackingList	new_plist_entry(void);
PackingList	last_plist(Package *);
PackingList	find_plist(Package *, plist_t);
char		*find_plist_option(Package *, char *name);
void		plist_delete(Package *, Boolean, plist_t, char *);
void		free_plist(Package *);
void		mark_plist(Package *);
void		csum_plist_entry(char *, PackingList);
void		add_plist(Package *, plist_t, char *);
void		add_plist_top(Package *, plist_t, char *);
void		delete_plist(Package *pkg, Boolean all, plist_t type, char *name);
void		write_plist(Package *, FILE *);
void		read_plist(Package *, FILE *);
int		plist_cmd(char *, char **);
int		delete_package(Boolean, Boolean, Package *);
Boolean 	make_preserve_name(char *, int, char *, char *);

/* For all */
int		pkg_perform(char **);

/* Externs */
extern Boolean	Verbose;
extern Boolean	Fake;
extern Boolean  Force;
extern int	AutoAnswer;

#endif /* _INST_LIB_LIB_H_ */
