/*
 * Miscellaneous support routines..
 *
 * $Id: misc.c,v 1.22.2.3 1996/12/12 22:57:01 jkh Exp $
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

#include "sysinstall.h"
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
#include <sys/reboot.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>

/* Quick check to see if a file is readable */
Boolean
file_readable(char *fname)
{
    if (!access(fname, F_OK))
	return TRUE;
    return FALSE;
}

/* Quick check to see if a file is executable */
Boolean
file_executable(char *fname)
{
    if (!access(fname, X_OK))
	return TRUE;
    return FALSE;
}

/* Concatenate two strings into static storage */
char *
string_concat(char *one, char *two)
{
    static char tmp[FILENAME_MAX];

    /* Yes, we're deliberately cavalier about not checking for overflow */
    strcpy(tmp, one);
    strcat(tmp, two);
    return tmp;
}

/* sane strncpy() function */
char *
sstrncpy(char *dst, const char *src, int size)
{
    dst[size] = '\0';
    return strncpy(dst, src, size);
}

/* Concatenate three strings into static storage */
char *
string_concat3(char *one, char *two, char *three)
{
    static char tmp[FILENAME_MAX];

    /* Yes, we're deliberately cavalier about not checking for overflow */
    strcpy(tmp, one);
    strcat(tmp, two);
    strcat(tmp, three);
    return tmp;
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

/* copy optionally and allow second arg to be null */
char *
string_copy(char *s1, char *s2)
{
    if (!s1)
	return NULL;
    if (!s2)
	s1[0] = '\0';
    else
	strcpy(s1, s2);
    return s1;
}

/* convert an integer to a string, using a static buffer */
char *
itoa(int value)
{
    static char buf[13];

    snprintf(buf, 12, "%d", value);
    return buf;
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

char *
pathBaseName(const char *path)
{
    char *pt;
    char *ret = (char *)path;

    pt = strrchr(path,(int)'/');

    if (pt != 0)			/* if there is a slash */
    {
	ret = ++pt;			/* start the file after it */
    }
    
    return(ret);
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
	msgFatal("Invalid malloc size of %d!", size);
    ptr = malloc(size);
    if (!ptr)
	msgFatal("Out of memory!");
    bzero(ptr, size);
    return ptr;
}

/* A realloc that checks errors */
void *
safe_realloc(void *orig, size_t size)
{
    void *ptr;

    if (size <= 0)
	msgFatal("Invalid realloc size of %d!", size);
    ptr = realloc(orig, size);
    if (!ptr)
	msgFatal("Out of memory!");
    return ptr;
}

/* Create a path biased from the VAR_INSTALL_ROOT variable (if not /) */
char *
root_bias(char *path)
{
    static char tmp[FILENAME_MAX];
    char *cp = variable_get(VAR_INSTALL_ROOT);

    if (!strcmp(cp, "/"))
	return path;
    strcpy(tmp, variable_get(VAR_INSTALL_ROOT));
    strcat(tmp, path);
    return tmp;
}

/*
 * These next routines are kind of specialized just for building string lists
 * for dialog_menu().
 */

/* Add an item to an item list */
dialogMenuItem *
item_add(dialogMenuItem *list, char *prompt, char *title,
	 int (*checked)(dialogMenuItem *self),
	 int (*fire)(dialogMenuItem *self),
	 void (*selected)(dialogMenuItem *self, int is_selected),
	 void *data, int aux, int *curr, int *max)
{
    dialogMenuItem *d;

    if (*curr == *max) {
	*max += 20;
	list = (dialogMenuItem *)realloc(list, sizeof(dialogMenuItem) * *max);
    }
    d = &list[(*curr)++];
    bzero(d, sizeof(*d));
    d->prompt = prompt ? strdup(prompt) : NULL;
    d->title = title ? strdup(title) : NULL;
    d->checked = checked;
    d->fire = fire;
    d->selected = selected;
    d->data = data;
    d->aux = aux;
    return list;
}

/* Toss the items out */
void
items_free(dialogMenuItem *list, int *curr, int *max)
{
    int i;

    for (i = 0; list[i].prompt; i++) {
	safe_free(list[i].prompt);
	safe_free(list[i].title);
    }
    safe_free(list);
    *curr = *max = 0;
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
    	sprintf(device, "/mnt/dev/%s", (char *)dev);
	sprintf(mountpoint, "/mnt%s", mountp);
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
    if (mount(MOUNT_UFS, mountpoint, MNT_ASYNC, (caddr_t)&ufsargs) == -1) {
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
