/*
 * Miscellaneous support routines..
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <sys/reboot.h>
#include <sys/disklabel.h>
#include <fs/msdosfs/msdosfsmount.h>

#include "sade.h"

/* Quick check to see if a file is readable */
Boolean
file_readable(char *fname)
{
    if (!access(fname, F_OK))
	return TRUE;
    return FALSE;
}

/* sane strncpy() function */
char *
sstrncpy(char *dst, const char *src, int size)
{
    dst[size] = '\0';
    return strncpy(dst, src, size);
}

/* Clip the whitespace off the end of a string */
char *
string_prune(char *str)
{
    int len = str ? strlen(str) : 0;

    while (len && isspace(str[len - 1]))
	str[--len] = '\0';
    return str;
}

/* run the whitespace off the front of a string */
char *
string_skipwhite(char *str)
{
    while (*str && isspace(*str))
	++str;
    return str;
}

Boolean
directory_exists(const char *dirname)
{
    DIR *tptr;

    if (!dirname)
	return FALSE;
    if (!strlen(dirname))
	return FALSE;

    tptr = opendir(dirname);
    if (!tptr)
	return (FALSE);

    closedir(tptr);
    return (TRUE);
}

/* A free guaranteed to take NULL ptrs */
void
safe_free(void *ptr)
{
    if (ptr)
	free(ptr);
}

/* A malloc that checks errors */
void *
safe_malloc(size_t size)
{
    void *ptr;

    if (size <= 0)
	msgFatal("Invalid malloc size of %ld!", (long)size);
    ptr = malloc(size);
    if (!ptr)
	msgFatal("Out of memory!");
    bzero(ptr, size);
    return ptr;
}

int
Mkdir(char *ipath)
{
    struct stat sb;
    int final;
    char *p, *path;

    if (file_readable(ipath) || Fake)
	return DITEM_SUCCESS;

    path = strcpy(alloca(strlen(ipath) + 1), ipath);
    if (isDebug())
	msgDebug("mkdir(%s)\n", path);
    p = path;
    if (p[0] == '/')		/* Skip leading '/'. */
	++p;
    for (final = FALSE; !final; ++p) {
	if (p[0] == '\0' || (p[0] == '/' && p[1] == '\0'))
	    final = TRUE;
	else if (p[0] != '/')
	    continue;
	*p = '\0';
	if (stat(path, &sb)) {
	    if (errno != ENOENT) {
		msgConfirm("Couldn't stat directory %s: %s", path, strerror(errno));
		return DITEM_FAILURE;
	    }
	    if (isDebug())
		msgDebug("mkdir(%s..)\n", path);
	    if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
		msgConfirm("Couldn't create directory %s: %s", path,strerror(errno));
		return DITEM_FAILURE;
	    }
	}
	*p = '/';
    }
    return DITEM_SUCCESS;
}

int
Mount(char *mountp, void *dev)
{
    struct ufs_args ufsargs;
    char device[80];
    char mountpoint[FILENAME_MAX];

    if (Fake)
	return DITEM_SUCCESS;

    if (*((char *)dev) != '/') {
    	sprintf(device, "/dev/%s", (char *)dev);
	sprintf(mountpoint, "%s", mountp);
    }
    else {
	strcpy(device, dev);
	strcpy(mountpoint, mountp);
    }
    memset(&ufsargs,0,sizeof ufsargs);

    if (Mkdir(mountpoint)) {
	msgConfirm("Unable to make directory mountpoint for %s!", mountpoint);
	return DITEM_FAILURE;
    }
    if (isDebug())
	msgDebug("mount %s %s\n", device, mountpoint);

    ufsargs.fspec = device;
    if (mount("ufs", mountpoint, 0,
	(caddr_t)&ufsargs) == -1) {
	msgConfirm("Error mounting %s on %s : %s", device, mountpoint, strerror(errno));
	return DITEM_FAILURE;
    }
    return DITEM_SUCCESS;
}

WINDOW *
savescr(void)
{
    WINDOW *w;

    w = dupwin(newscr);
    return w;
}

void
restorescr(WINDOW *w)
{
    touchwin(w);
    wrefresh(w);
    delwin(w);
}

static int
xdialog_count_rows(const char *p)
{
	int rows = 0;

	while ((p = strchr(p, '\n')) != NULL) {
		p++;
		if (*p == '\0')
			break;
		rows++;
	}

	return rows ? rows : 1;
}

static int
xdialog_count_columns(const char *p)
{
	int len;
	int max_len = 0;
	const char *q;

	for (; (q = strchr(p, '\n')) != NULL; p = q + 1) {
		len = q - p;
		max_len = MAX(max_len, len);
	}

	len = strlen(p);
	max_len = MAX(max_len, len);
	return max_len;
}

int
xdialog_menu(const char *title, const char *cprompt, int height, int width,
	     int menu_height, int item_no, dialogMenuItem *ditems)
{
	int i, result, choice = 0;
	DIALOG_LISTITEM *listitems;
	DIALOG_VARS save_vars;

	dlg_save_vars(&save_vars);

	/* initialize list items */
	listitems = dlg_calloc(DIALOG_LISTITEM, item_no + 1);
	assert_ptr(listitems, "xdialog_menu");
	for (i = 0; i < item_no; i++) {
		listitems[i].name = ditems[i].prompt;
		listitems[i].text = ditems[i].title;
	}

	/* calculate height */
	if (height < 0)
		height = xdialog_count_rows(cprompt) + menu_height + 4 + 2;
	if (height > LINES)
		height = LINES;

	/* calculate width */
	if (width < 0) {
		int tag_x = 0;

		for (i = 0; i < item_no; i++) {
			int j, l;

			l = strlen(listitems[i].name);
			for (j = 0; j < item_no; j++) {
				int k = strlen(listitems[j].text);
				tag_x = MAX(tag_x, l + k + 2);
			}
		}
		width = MAX(xdialog_count_columns(cprompt), title != NULL ? xdialog_count_columns(title) : 0);
		width = MAX(width, tag_x + 4) + 4;
	}
	width = MAX(width, 24);
	if (width > COLS)
		width = COLS;

	/* show menu */
	dialog_vars.default_item = listitems[choice].name;
	result = dlg_menu(title, cprompt, height, width,
	    menu_height, item_no, listitems, &choice, NULL);
	switch (result) {
	case DLG_EXIT_ESC:
		result = -1;
		break;
	case DLG_EXIT_OK:
		if (ditems[choice].fire != NULL) {
			int status;
			WINDOW *save;

			save = savescr();
			status = ditems[choice].fire(ditems + choice);
			restorescr(save);
		}
		result = 0;
		break;
	case DLG_EXIT_CANCEL:
	default:
		result = 1;
		break;
	}

	free(listitems);
	dlg_restore_vars(&save_vars);
	return result;
}

int
xdialog_radiolist(const char *title, const char *cprompt, int height, int width,
		  int menu_height, int item_no, dialogMenuItem *ditems)
{
	int i, result, choice = 0;
	DIALOG_LISTITEM *listitems;
	DIALOG_VARS save_vars;

	dlg_save_vars(&save_vars);

	/* initialize list items */
	listitems = dlg_calloc(DIALOG_LISTITEM, item_no + 1);
	assert_ptr(listitems, "xdialog_menu");
	for (i = 0; i < item_no; i++) {
		listitems[i].name = ditems[i].prompt;
		listitems[i].text = ditems[i].title;
		listitems[i].state = i == choice;
	}

	/* calculate height */
	if (height < 0)
		height = xdialog_count_rows(cprompt) + menu_height + 4 + 2;
	if (height > LINES)
		height = LINES;

	/* calculate width */
	if (width < 0) {
		int check_x = 0;

		for (i = 0; i < item_no; i++) {
			int j, l;

			l = strlen(listitems[i].name);
			for (j = 0; j < item_no; j++) {
				int k = strlen(listitems[j].text);
				check_x = MAX(check_x, l + k + 6);
			}
		}
		width = MAX(xdialog_count_columns(cprompt), title != NULL ? xdialog_count_columns(title) : 0);
		width = MAX(width, check_x + 4) + 4;
	}
	width = MAX(width, 24);
	if (width > COLS)
		width = COLS;

	/* show menu */
	dialog_vars.default_item = listitems[choice].name;
	result = dlg_checklist(title, cprompt, height, width,
	    menu_height, item_no, listitems, NULL, FLAG_RADIO, &choice);
	switch (result) {
	case DLG_EXIT_ESC:
		result = -1;
		break;
	case DLG_EXIT_OK:
		if (ditems[choice].fire != NULL) {
			int status;
			WINDOW *save;

			save = savescr();
			status = ditems[choice].fire(ditems + choice);
			restorescr(save);
		}
		result = 0;
		break;
	case DLG_EXIT_CANCEL:
	default:
		result = 1;
		break;
	}

	/* save result */
	if (result == 0)
		dlg_add_result(listitems[choice].name);
	free(listitems);
	dlg_restore_vars(&save_vars);
	return result;
}

int
xdialog_msgbox(const char *title, const char *cprompt,
	       int height, int width, int pauseopt)
{
	/* calculate height */
	if (height < 0)
		height = 2 + xdialog_count_rows(cprompt) + 2 + !!pauseopt;
	if (height > LINES)
		height = LINES;

	/* calculate width */
	if (width < 0) {
		width = title != NULL ? xdialog_count_columns(title) : 0;
		width = MAX(width, xdialog_count_columns(cprompt)) + 4;
	}
	if (pauseopt)
		width = MAX(width, 10);
	if (width > COLS)
		width = COLS;

	return dialog_msgbox(title, cprompt, height, width, pauseopt);
}
