/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: index.c,v 1.50 1997/05/05 08:38:12 jkh Exp $
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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <dialog.h>
#include "sysinstall.h"

/* Macros and magic values */
#define MAX_MENU	12
#define _MAX_DESC	55

static int	index_extract_one(Device *dev, PkgNodePtr top, PkgNodePtr who, Boolean depended);

/* Smarter strdup */
inline char *
_strdup(char *ptr)
{
    return ptr ? strdup(ptr) : NULL;
}

static char *descrs[] = {
    "Package Selection", "To mark a package, move to it and press SPACE.  If the package is\n"
    "already marked, it will be unmarked or deleted (if installed).\n"
    "To search for a package by name, press ESC.  To select a category,\n"
    "press RETURN.  NOTE:  The All category selection creates a very large\n"
    "submenu.  If you select it, please be patient while it comes up.",
    "Package Targets", "These are the packages you've selected for extraction.\n\n"
    "If you're sure of these choices, select OK.\n"
    "If not, select Cancel to go back to the package selection menu.\n",
    "All", "All available packages in all categories.",
    "applications", "User application software.",
    "astro", "Applications related to astronomy.",
    "archivers", "Utilities for archiving and unarchiving data.",
    "audio", "Audio utilities - most require a supported sound card.",
    "benchmarks", "Utilities for measuring system performance.",
    "benchmarking", "Utilities for measuring system performance.",
    "cad", "Computer Aided Design utilities.",
    "chinese", "Ported software for the Chinese market.",
    "comms", "Communications utilities.",
    "converters", "Format conversion utilities..",
    "databases", "Database software.",
    "devel", "Software development utilities and libraries.",
    "development", "Software development utilities and libraries.",
    "documentation", "Document preparation utilities.",
    "editors", "Common text editors.",
    "emulation", "Utilities for emulating other OS types.",
    "emulators", "Utilities for emulating other OS types.",
    "games", "Various and sundry amusements.",
    "graphics", "Graphics libraries and utilities.",
    "japanese", "Ported software for the Japanese market.",
    "korean", "Ported software for the Korean market.",
    "lang", "Computer languages.",
    "languages", "Computer languages.",
    "libraries", "Software development libraries.",
    "mail", "Electronic mail packages and utilities.",
    "math", "Mathematical computation software.",
    "mbone", "Applications and utilities for the mbone.",
    "misc", "Miscellaneous utilities.",
    "net", "Networking utilities.",
    "networking", "Networking utilities.",
    "news", "USENET News support software.",
    "numeric", "Mathematical computation software.",
    "orphans", "Packages without a home elsewhere.",
    "perl5", "Utilities/modules for the PERL5 language..",
    "plan9", "Software from the plan9 Operating System.",
    "print", "Utilities for dealing with printing.",
    "printing", "Utilities for dealing with printing.",
    "programming", "Software development utilities and libraries.",
    "russian", "Ported software for the Russian market.",
    "security", "System security software.",
    "shells", "Various shells (tcsh, bash, etc).",
    "sysutils", "Various system utilities.",
    "textproc", "Text processing/search utilities.",
    "tcl75", "TCL v7.5 and packages which depend on it.",
    "tcl76", "TCL v7.6 and packages which depend on it.",
    "tk41", "Tk4.1 and packages which depend on it.",
    "tk42", "Tk4.2 and packages which depend on it.",
    "tk80", "Tk8.0 and packages which depend on it.",
    "troff", "TROFF Text formatting utilities.",
    "utils", "Various user utilities.",
    "utilities", "Various user utilities.",
    "vietnamese", "Ported software for the Vietnamese market.",
    "www", "WEB utilities (browers, HTTP servers, etc).",
    "x11", "X Window System based utilities.",
    NULL, NULL,
};

static char *
fetch_desc(char *name)
{
    int i;

    for (i = 0; descrs[i]; i += 2) {
	if (!strcmp(descrs[i], name))
	    return descrs[i + 1];
    }
    return "No description provided";
}

static PkgNodePtr
new_pkg_node(char *name, node_type type)
{
    PkgNodePtr tmp = safe_malloc(sizeof(PkgNode));

    tmp->name = _strdup(name);
    tmp->type = type;
    return tmp;
}

static char *
strip(char *buf)
{
    int i;

    for (i = 0; buf[i]; i++)
	if (buf[i] == '\t' || buf[i] == '\n')
	    buf[i] = ' ';
    return buf;
}

static IndexEntryPtr
new_index(char *name, char *pathto, char *prefix, char *comment, char *descr, char *maint, char *deps)
{
    IndexEntryPtr tmp = safe_malloc(sizeof(IndexEntry));

    tmp->name =		_strdup(name);
    tmp->path =		_strdup(pathto);
    tmp->prefix =	_strdup(prefix);
    tmp->comment =	_strdup(comment);
    tmp->descrfile =	strip(_strdup(descr));
    tmp->maintainer =	_strdup(maint);
    tmp->deps =		_strdup(deps);
    return tmp;
}

static void
index_register(PkgNodePtr top, char *where, IndexEntryPtr ptr)
{
    PkgNodePtr p, q;

    for (q = NULL, p = top->kids; p; p = p->next) {
	if (!strcmp(p->name, where)) {
	    q = p;
	    break;
	}
    }
    if (!p) {
	/* Add new category */
	q = new_pkg_node(where, PLACE);
	q->desc = fetch_desc(where);
	q->next = top->kids;
	top->kids = q;
    }
    p = new_pkg_node(ptr->name, PACKAGE);
    p->desc = ptr->comment;
    p->data = ptr;
    p->next = q->kids;
    q->kids = p;
}

static int
copy_to_sep(char *to, char *from, int sep)
{
    char *tok;

    tok = strchr(from, sep);
    if (!tok) {
	*to = '\0';
	return 0;
    }
    *tok = '\0';
    strcpy(to, from);
    return tok + 1 - from;
}

static int
readline(FILE *fp, char *buf, int max)
{
    int rv, i = 0;
    char ch;

    while ((rv = fread(&ch, 1, 1, fp)) == 1 && ch != '\n' && i < max)
	buf[i++] = ch;
    if (i < max)
	buf[i] = '\0';
    return rv;
}

int
index_parse(FILE *fp, char *name, char *pathto, char *prefix, char *comment, char *descr, char *maint, char *cats, char *rdeps)
{
    char line[1024];
    char junk[256];
    char *cp;
    int i;

    i = readline(fp, line, 1024);
    if (i <= 0)
	return EOF;
    cp = line;
    cp += copy_to_sep(name, cp, '|');
    cp += copy_to_sep(pathto, cp, '|');
    cp += copy_to_sep(prefix, cp, '|');
    cp += copy_to_sep(comment, cp, '|');
    cp += copy_to_sep(descr, cp, '|');
    cp += copy_to_sep(maint, cp, '|');
    cp += copy_to_sep(cats, cp, '|');
    cp += copy_to_sep(junk, cp, '|');	/* build deps - not used */
    if (index(cp, '|'))
	copy_to_sep(rdeps, cp, '|');
    else
	strncpy(rdeps, cp, 510);
    return 0;
}

int
index_read(FILE *fp, PkgNodePtr papa)
{
    char name[127], pathto[255], prefix[255], comment[255], descr[127], maint[127], cats[511], deps[511];

    while (index_parse(fp, name, pathto, prefix, comment, descr, maint, cats, deps) != EOF) {
	char *cp, *cp2, tmp[511];
	IndexEntryPtr idx;

	idx = new_index(name, pathto, prefix, comment, descr, maint, deps);
	/* For now, we only add things to menus if they're in categories.  Keywords are ignored */
	for (cp = strcpy(tmp, cats); (cp2 = strchr(cp, ' ')) != NULL; cp = cp2 + 1) {
	    *cp2 = '\0';
	    index_register(papa, cp, idx);
	}
	index_register(papa, cp, idx);

	/* Add to special "All" category */
	index_register(papa, "All", idx);
    }
    return 0;
}

void
index_init(PkgNodePtr top, PkgNodePtr plist)
{
    if (top) {
	top->next = top->kids = NULL;
	top->name = "Package Selection";
	top->type = PLACE;
	top->desc = fetch_desc(top->name);
	top->data = NULL;
    }
    if (plist) {
	plist->next = plist->kids = NULL;
	plist->name = "Package Targets";
	plist->type = PLACE;
	plist->desc = fetch_desc(plist->name);
	plist->data = NULL;
    }
}

void
index_print(PkgNodePtr top, int level)
{
    int i;

    while (top) {
	for (i = 0; i < level; i++) putchar('\t');
	printf("name [%s]: %s\n", top->type == PLACE ? "place" : "package", top->name);
	for (i = 0; i < level; i++) putchar('\t');
	printf("desc: %s\n", top->desc);
	if (top->kids)
	    index_print(top->kids, level + 1);
	top = top->next;
    }
}

/* Swap one node for another */
static void
swap_nodes(PkgNodePtr a, PkgNodePtr b)
{
    PkgNode tmp;

    tmp = *a;
    *a = *b;
    a->next = tmp.next;
    tmp.next = b->next;
    *b = tmp;
}

/* Use a disgustingly simplistic bubble sort to put our lists in order */
void
index_sort(PkgNodePtr top)
{
    PkgNodePtr p, q;

    /* Sort everything at the top level */
    for (p = top->kids; p; p = p->next) {
	for (q = top->kids; q; q = q->next) {
	    if (q->next && strcmp(q->name, q->next->name) > 0)
		swap_nodes(q, q->next);
	}
    }

    /* Now sub-sort everything n levels down */
    
    for (p = top->kids; p; p = p->next) {
	if (p->kids)
	    index_sort(p);
    }
}

/* Delete an entry out of the list it's in (only the plist, at present) */
void
index_delete(PkgNodePtr n)
{
    if (n->next) {
	PkgNodePtr p = n->next;

	*n = *(n->next);
	safe_free(p);
    }
    else /* Kludgy end sentinal */
	n->name = NULL;
}

/*
 * Search for a given node by name, returning the category in if
 * tp is non-NULL.
 */
PkgNodePtr
index_search(PkgNodePtr top, char *str, PkgNodePtr *tp)
{
    PkgNodePtr p, sp;

    for (p = top->kids; p && p->name; p = p->next) {
	/* Subtract out the All category from searches */
	if (!strcmp(p->name, "All"))
	    continue;

	/* If tp == NULL, we're looking for an exact package match */
	if (!tp && !strncmp(p->name, str, strlen(str)))
	    return p;

	/* If tp, we're looking for both a package and a pointer to the place it's in */
	if (tp && !strncmp(p->name, str, strlen(str))) {
	    *tp = top;
	    return p;
	}

	/* The usual recursion-out-of-laziness ploy */
	if (p->kids)
	    if ((sp = index_search(p, str, tp)) != NULL)
		return sp;
    }
    if (p && !p->name)
	p = NULL;
    return p;
}

int
pkg_checked(dialogMenuItem *self)
{
    PkgNodePtr kp = self->data, plist = (PkgNodePtr)self->aux;
    int i;

    i = index_search(plist, kp->name, NULL) ? TRUE : FALSE;
    if (kp->type == PACKAGE && plist)
	return i || package_exists(kp->name);
    else
	return FALSE;
}

int
pkg_fire(dialogMenuItem *self)
{
    int ret;
    PkgNodePtr sp, kp = self->data, plist = (PkgNodePtr)self->aux;

    if (!plist)
	ret = DITEM_FAILURE;
    else if (kp->type == PACKAGE) {
	sp = index_search(plist, kp->name, NULL);
	/* Not already selected? */
	if (!sp) {
	    if (!package_exists(kp->name)) {
		PkgNodePtr np = (PkgNodePtr)safe_malloc(sizeof(PkgNode));

		*np = *kp;
		np->next = plist->kids;
		plist->kids = np;
		msgInfo("Added %s to selection list", kp->name);
	    }
	    else {
		WINDOW *save = savescr();

		if (!msgYesNo("Do you really want to delete %s from the system?", kp->name))
		    if (vsystem("pkg_delete %s %s", isDebug() ? "-v" : "", kp->name))
			msgConfirm("Warning:  pkg_delete of %s failed.\n  Check debug output for details.", kp->name);
		restorescr(save);
	    }
	}
	else {
	    msgInfo("Removed %s from selection list", kp->name);
	    index_delete(sp);
	}
	ret = DITEM_SUCCESS;
    }
    else {	/* Not a package, must be a directory */
	int p, s;
		    
	p = s = 0;
	index_menu(kp, plist, &p, &s);
	ret = DITEM_SUCCESS | DITEM_CONTINUE;
    }
    return ret;
}

void
pkg_selected(dialogMenuItem *self, int is_selected)
{
    PkgNodePtr kp = self->data;

    if (!is_selected || kp->type != PACKAGE)
	return;
    msgInfo(kp->desc);
}

int
index_menu(PkgNodePtr top, PkgNodePtr plist, int *pos, int *scroll)
{
    int n, rval, maxname;
    int curr, max;
    PkgNodePtr kp;
    dialogMenuItem *nitems;
    Boolean hasPackages;
    WINDOW *w;

    hasPackages = FALSE;
    nitems = NULL;

    w = savescr();
    n = maxname = 0;
    /* Figure out if this menu is full of "leaves" or "branches" */
    for (kp = top->kids; kp && kp->name; kp = kp->next) {
	int len;

	++n;
	if (kp->type == PACKAGE && plist) {
	    hasPackages = TRUE;
	    if ((len = strlen(kp->name)) > maxname)
		maxname = len;
	}
    }
    if (!n && plist) {
	msgConfirm("The %s menu is empty.", top->name);
	restorescr(w);
	return DITEM_LEAVE_MENU;
    }

    while (1) {
	n = 0;
	curr = max = 0;
	use_helpline(NULL);
	use_helpfile(NULL);
	kp = top->kids;
	if (!hasPackages && plist) {
	    nitems = item_add(nitems, "OK", NULL, NULL, NULL, NULL, NULL, 0, &curr, &max);
	    nitems = item_add(nitems, "Install", NULL, NULL, NULL, NULL, NULL, 0, &curr, &max);
	}
	while (kp && kp->name) {
	    char buf[256];
	    IndexEntryPtr ie = kp->data;

	    /* Brutally adjust description to fit in menu */
	    if (kp->type == PACKAGE)
		snprintf(buf, sizeof buf, "[%s]", ie->path ? ie->path : "External vendor");
	    else
		SAFE_STRCPY(buf, kp->desc);
	    if (strlen(buf) > (_MAX_DESC - maxname))
		buf[_MAX_DESC - maxname] = '\0';
	    nitems = item_add(nitems, kp->name, buf, pkg_checked, pkg_fire, pkg_selected, kp, (int)plist, &curr, &max);
	    ++n;
	    kp = kp->next;
	}
	/* NULL delimiter so item_free() knows when to stop later */
	nitems = item_add(nitems, NULL, NULL, NULL, NULL, NULL, NULL, 0, &curr, &max);

recycle:
	dialog_clear_norefresh();
	if (hasPackages)
	    rval = dialog_checklist(top->name, top->desc, -1, -1, n > MAX_MENU ? MAX_MENU : n, -n, nitems, NULL);
	else
	    rval = dialog_menu(top->name, top->desc, -1, -1, n > MAX_MENU ? MAX_MENU : n, -n, nitems + (plist ? 2 : 0), (char *)plist, pos, scroll);
	if (rval == -1 && plist) {
	    static char *cp;
	    PkgNodePtr menu;

	    /* Search */
	    if ((cp = msgGetInput(cp, "Search by package name.  Please enter search string:")) != NULL) {
		PkgNodePtr p = index_search(top, cp, &menu);

		if (p) {
		    int pos, scroll;

		    /* These need to be set to point at the found item, actually.  Hmmm! */
		    pos = scroll = 0;
		    index_menu(menu, plist, &pos, &scroll);
		}
		else
		    msgConfirm("Search string: %s yielded no hits.", cp);
	    }
	    goto recycle;
	}
	items_free(nitems, &curr, &max);
	restorescr(w);
	return rval ? DITEM_FAILURE : DITEM_SUCCESS;
    }
}

int
index_extract(Device *dev, PkgNodePtr top, PkgNodePtr plist)
{
    PkgNodePtr tmp;
    int status = DITEM_SUCCESS;

    for (tmp = plist->kids; tmp && tmp->name; tmp = tmp->next)
	status = index_extract_one(dev, top, tmp, FALSE);
    return status;
}

static int
index_extract_one(Device *dev, PkgNodePtr top, PkgNodePtr who, Boolean depended)
{
    int status = DITEM_SUCCESS;
    PkgNodePtr tmp2;
    IndexEntryPtr id = who->data;

    if (id && id->deps && strlen(id->deps)) {
	char t[1024], *cp, *cp2;

	SAFE_STRCPY(t, id->deps);
	cp = t;
	while (cp && DITEM_STATUS(status) == DITEM_SUCCESS) {
	    if ((cp2 = index(cp, ' ')) != NULL)
		*cp2 = '\0';
	    if ((tmp2 = index_search(top, cp, NULL)) != NULL) {
		status = index_extract_one(dev, top, tmp2, TRUE);
		if (DITEM_STATUS(status) != DITEM_SUCCESS) {
		    if (variable_get(VAR_NO_CONFIRM))
			msgNotify("Loading of dependant package %s failed", cp);
		    else
			msgConfirm("Loading of dependant package %s failed", cp);
		}
	    }
	    if (cp2)
		cp = cp2 + 1;
	    else
		cp = NULL;
	}
    }
    /* Done with the deps?  Load the real m'coy */
    if (DITEM_STATUS(status) == DITEM_SUCCESS)
	status = package_extract(dev, who->name, depended);
    return status;
}
