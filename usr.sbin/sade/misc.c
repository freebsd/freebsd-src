/*
 * Miscellaneous support routines..
 *
 * $Id: misc.c,v 1.34 1997/04/03 13:44:59 jkh Exp $
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
#include <ufs/ufs/ufsmount.h>
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
 * These next routines are kind of specialized just for building item lists
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
    	sprintf(device, "%s/dev/%s", RunningAsInit ? "/mnt" : "", (char *)dev);
	sprintf(mountpoint, "%s%s", RunningAsInit ? "/mnt" : "", mountp);
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
openLayoutDialog(char *helpfile, char *title, int x, int y, int width, int height)
{
    WINDOW		*win;
    static char		help[FILENAME_MAX];

    /* We need a curses window */
    win = newwin(LINES, COLS, 0, 0);
    if (win) {
	/* Say where our help comes from */
	if (helpfile) {
	    use_helpline("Press F1 for more information on this screen.");
	    use_helpfile(systemHelpFile(helpfile, help));
	}
	/* Setup a nice screen for us to splat stuff onto */
	draw_box(win, y, x, height, width, dialog_attr, border_attr);
	wattrset(win, dialog_attr);
	mvwaddstr(win, y, x + (COLS - strlen(title)) / 2, title);
    }
    return win;
}

ComposeObj *
initLayoutDialog(WINDOW *win, Layout *layout, int x, int y, int *max)
{
    ComposeObj *obj = NULL, *first;
    int n;

    /* Loop over the layout list, create the objects, and add them
       onto the chain of objects that dialog uses for traversal*/
    
    n = 0;
    while (layout[n].help != NULL) {
	int t = TYPE_OF_OBJ(layout[n].type);

	switch (t) {
	case STRINGOBJ:
	    layout[n].obj = NewStringObj(win, layout[n].prompt, layout[n].var,
					 layout[n].y + y, layout[n].x + x, layout[n].len, layout[n].maxlen);
	    ((StringObj *)layout[n].obj)->attr_mask = ATTR_OF_OBJ(layout[n].type);
	    break;
	    
	case BUTTONOBJ:
	    layout[n].obj = NewButtonObj(win, layout[n].prompt, layout[n].var, layout[n].y + y, layout[n].x + x);
	    break;
	    
	default:
	    msgFatal("Don't support this object yet!");
	}
	AddObj(&obj, t, (void *) layout[n].obj);
	n++;
    }
    *max = n - 1;
    /* Find the first object in the list */
    for (first = obj; first->prev; first = first->prev);
    return first;
}

int
layoutDialogLoop(WINDOW *win, Layout *layout, ComposeObj **obj, int *n, int max, int *cbutton, int *cancel)
{
    char help_line[80];
    int ret, i, len = strlen(layout[*n].help);

    /* Display the help line at the bottom of the screen */
    for (i = 0; i < 79; i++)
	help_line[i] = (i < len) ? layout[*n].help[i] : ' ';
    help_line[i] = '\0';
    use_helpline(help_line);
    display_helpline(win, LINES - 1, COLS - 1);
    wrefresh(win);
	    
    /* Ask for libdialog to do its stuff */
    ret = PollObj(obj);
    /* Handle special case stuff that libdialog misses. Sigh */
    switch (ret) {
    case SEL_ESC:	/* Bail out */
	*cancel = TRUE;
	return FALSE;
	      
	/* This doesn't work for list dialogs. Oh well. Perhaps
	   should special case the move from the OK button ``up''
	   to make it go to the interface list, but then it gets
	   awkward for the user to go back and correct screw up's
	   in the per-interface section */
    case KEY_DOWN:
    case SEL_CR:
    case SEL_TAB:
	if (*n < max)
	    ++*n;
	else
	    *n = 0;
	break;
	      
	/* The user has pressed enter over a button object */
    case SEL_BUTTON:
	if (cbutton && *cbutton)
	    *cancel = TRUE;
	else
	    *cancel = FALSE;
	return FALSE;
	
    case KEY_UP:
    case SEL_BACKTAB:
	if (*n)
	    --*n;
	else
	    *n = max;
	break;
	
    case KEY_F(1):
	display_helpfile();
	
	/* They tried some key combination we don't support - tootle them forcefully! */
    default:
	beep();
    }
    return TRUE;
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

