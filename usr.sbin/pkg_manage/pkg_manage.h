/*
 * include file for pkg_manage
 *
 *
 * Copyright (c) 1995, Marc van Kempen
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <ncurses.h>
#include <dialog.h>

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

typedef struct {
    int		N;		/* size of buf in bytes */
    int		Nitems;		/* # of packages */
    char	*buf;		/* buffer to hold package info */
    char	**name;		/* array of char pointer to locations in buf */
    char	**comment;	/* idem */
    char 	**description;  /* idem */
    unsigned char **mnu;	/* pointers to be used with dialog_menu() */
} PKG_info;

/* Names of the description and comment files in the package */
#define DESC		"+DESC"
#define COMMENT		"+COMMENT"
#define CONTENTS	"+CONTENTS"

/* some programs */
#define PKG_DELETE 	"/usr/sbin/pkg_delete"
#define PKG_INFO 	"/usr/sbin/pkg_info"
#define PKG_ADD 	"/usr/sbin/pkg_add"
#define TAR 		"/usr/bin/tar"
#define GUNZIP		"/usr/bin/gunzip"

/* HELP_PATH must have a trailing '/' */
#ifndef HELP_PATH
#define HELP_PATH	"/home/marc/src/dialog/pkg_manage/"
#endif
#define VIEW_INST_HLP 	HELP_PATH##"pkg_view_inst.hlp"
#define DEL_INST_HLP 	HELP_PATH##"pkg_del_inst.hlp"
#define INSTALL_HLP	HELP_PATH##"pkg_install.hlp"
#define MAIN_HLP 	HELP_PATH##"pkg_main.hlp"
#define DS_INSTALL_HLP  HELP_PATH##"pkg_ds_install.hlp"
#define PREVIEW_HLP	HELP_PATH##"pkg_preview.hlp"
#define PREVIEW_FS_HLP  HELP_PATH##"pkg_preview_fs.hlp"

/*
 * prototypes
 */

void	run_menu(void);
void	get_pkginfo(void);
void    FreeInfo(void);
void	install_package(char *fname);
int	get_pkg_index(char *selection);
void	install_batch(void);
int	get_desc(char *fname, char **name, char **comment,
		 char **desc, long *size, char *tmp_file);
int	exec_catch_errors(char *prog, char *arg, char *fout);
int	already_installed(char *name);
void    install_pkgs_indir(void);
