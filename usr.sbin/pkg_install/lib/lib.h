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
 * Include and define various things wanted by the library routines.
 *
 */

#ifndef _INST_LIB_LIB_H_
#define _INST_LIB_LIB_H_

/* Includes */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/queue.h>
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
#define DEF_LOG_DIR	"/var/db/pkg"
/* just in case we change the environment variable name */
#define PKG_DBDIR	"PKG_DBDIR"
/* macro to get name of directory where we put logging information */
#define LOG_DIR		(getenv(PKG_DBDIR) ? getenv(PKG_DBDIR) : DEF_LOG_DIR)

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

/* Version numbers to assist with changes in package file format */
#define PLIST_FMT_VER_MAJOR	1
#define PLIST_FMT_VER_MINOR	1

enum _plist_t {
    PLIST_FILE, PLIST_CWD, PLIST_CMD, PLIST_CHMOD,
    PLIST_CHOWN, PLIST_CHGRP, PLIST_COMMENT, PLIST_IGNORE,
    PLIST_NAME, PLIST_UNEXEC, PLIST_SRC, PLIST_DISPLAY,
    PLIST_PKGDEP, PLIST_MTREE, PLIST_DIR_RM, PLIST_IGNORE_INST,
    PLIST_OPTION, PLIST_ORIGIN, PLIST_DEPORIGIN
};
typedef enum _plist_t plist_t;

enum _match_t {
    MATCH_ALL, MATCH_EXACT, MATCH_GLOB, MATCH_REGEX
};
typedef enum _match_t match_t;

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
    char *name;
    char *origin;
    int fmtver_maj, fmtver_mnr;
};
typedef struct _pack Package;

struct reqr_by_entry {
    STAILQ_ENTRY(reqr_by_entry) link;
    char pkgname[PATH_MAX];
};
STAILQ_HEAD(reqr_by_head, reqr_by_entry);
                
/* Prototypes */
/* Misc */
int		vsystem(const char *, ...);
char		*vpipe(const char *, ...);
void		cleanup(int);
char		*make_playpen(char *, off_t);
char		*where_playpen(void);
void		leave_playpen(void);
off_t		min_free(const char *);

/* String */
char 		*get_dash_string(char **);
char		*copy_string(const char *);
char		*copy_string_plus_newline(const char *);
Boolean		suffix(const char *, const char *);
void		nuke_suffix(char *);
void		str_lowercase(char *);
char		*strconcat(const char *, const char *);
char		*get_string(char *, int, FILE *);

/* File */
Boolean		fexists(const char *);
Boolean		isdir(const char *);
Boolean		isemptydir(const char *fname);
Boolean		isemptyfile(const char *fname);
Boolean         isfile(const char *);
Boolean		isempty(const char *);
Boolean		issymlink(const char *);
Boolean		isURL(const char *);
char		*fileGetURL(const char *, const char *);
char		*fileFindByPath(const char *, const char *);
char		*fileGetContents(const char *);
void		write_file(const char *, const char *);
void		copy_file(const char *, const char *, const char *);
void		move_file(const char *, const char *, const char *);
void		copy_hierarchy(const char *, const char *, Boolean);
int		delete_hierarchy(const char *, Boolean, Boolean);
int		unpack(const char *, const char *);
void		format_cmd(char *, const char *, const char *, const char *);

/* Msg */
void		upchuck(const char *);
void		barf(const char *, ...);
void		whinge(const char *, ...);
Boolean		y_or_n(Boolean, const char *, ...);

/* Packing list */
PackingList	new_plist_entry(void);
PackingList	last_plist(Package *);
PackingList	find_plist(Package *, plist_t);
char		*find_plist_option(Package *, const char *name);
void		plist_delete(Package *, Boolean, plist_t, const char *);
void		free_plist(Package *);
void		mark_plist(Package *);
void		csum_plist_entry(char *, PackingList);
void		add_plist(Package *, plist_t, const char *);
void		add_plist_top(Package *, plist_t, const char *);
void		delete_plist(Package *pkg, Boolean all, plist_t type, const char *name);
void		write_plist(Package *, FILE *);
void		read_plist(Package *, FILE *);
int		plist_cmd(const char *, char **);
int		delete_package(Boolean, Boolean, Package *);
Boolean 	make_preserve_name(char *, int, const char *, const char *);

/* For all */
int		pkg_perform(char **);

/* Query installed packages */
char		**matchinstalled(match_t, char **, int *);
char		**matchbyorigin(const char *, int *);
int		isinstalledpkg(const char *name);

/* Dependencies */
int		sortdeps(char **);
int		chkifdepends(const char *, const char *);
int		requiredby(const char *, struct reqr_by_head **, Boolean, Boolean);

/* Version */
int		verscmp(Package *, int, int);

/* Externs */
extern Boolean	Verbose;
extern Boolean	Fake;
extern Boolean  Force;
extern int	AutoAnswer;

#endif /* _INST_LIB_LIB_H_ */
