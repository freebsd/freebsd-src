/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: index.c,v 1.20 1995/11/12 20:47:12 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
#define MAX_MENU	13
#define _MAX_DESC	62

static int	index_extract_one(Device *dev, PkgNodePtr top, PkgNodePtr who);

/* Smarter strdup */
inline char *
_strdup(char *ptr)
{
    return ptr ? strdup(ptr) : NULL;
}

static char *descrs[] = {
    "Package Selection", "To mark a package or select a category, move to it and press SPACE.\n"
    "To unmark a package, press SPACE again.  When you want to commit your\n"
    "marks, press [ENTER].  To go to a previous menu, select UP item or Cancel.\n"
    "To search for a package by name, press ESC.  To extract packages, you\n"
    "should Cancel all the way out of any submenus and finally this menu.",
    "Package Targets", "These are the packages you've selected for extraction.\n\n"
    "If you're sure of these choices, select OK.\n"
    "If not, select Cancel to go back to the package selection menu.\n",
    "All", "All available packages in all categories.",
    "applications", "User application software.",
    "archivers", "Utilities for archiving and unarchiving data.",
    "audio", "Audio utilities - most require a supported sound card.",
    "benchmarks", "Utilities for measuring system performance.",
    "benchmarking", "Utilities for measuring system performance.",
    "cad", "Computer Aided Design utilities.",
    "comms", "Communications utilities.",
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
    "lang", "Computer languages.",
    "languages", "Computer languages.",
    "libraries", "Software development libraries.",
    "mail", "Electronic mail packages and utilities.",
    "math", "Mathematical computation software.",
    "misc", "Miscellaneous utilities.",
    "net", "Networking utilities.",
    "networking", "Networking utilities.",
    "news", "USENET News support software.",
    "numeric", "Mathematical computation software.",
    "orphans", "Packages without a home elsewhere.",
    "plan9", "Software from the plan9 Operating System.",
    "print", "Utilities for dealing with printing.",
    "printing", "Utilities for dealing with printing.",
    "programming", "Software development utilities and libraries.",
    "russian", "Ported software for the Russian market.",
    "security", "System security software.",
    "shells", "Various shells (tcsh, bash, etc).",
    "sysutils", "Various system utilities.",
    "www", "WEB utilities (browers, HTTP servers, etc).",
    "troff", "TROFF Text formatting utilities.",
    "utils", "Various user utilities.",
    "utilities", "Various user utilities.",
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

static IndexEntryPtr
new_index(char *name, char *pathto, char *prefix, char *comment, char *descr, char *maint, char *deps)
{
    IndexEntryPtr tmp = safe_malloc(sizeof(IndexEntry));

    tmp->name =		_strdup(name);
    tmp->path =		_strdup(pathto);
    tmp->prefix =	_strdup(prefix);
    tmp->comment =	_strdup(comment);
    tmp->descrfile =	_strdup(descr);
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
	fprintf(stderr, "missing '%c' token.\n", sep);
	*to = '\0';
	return 0;
    }
    *tok = '\0';
    strcpy(to, from);
    return tok + 1 - from;
}

static int
readline(int fd, char *buf, int max)
{
    int rv, i = 0;
    char ch;

    while ((rv = read(fd, &ch, 1)) == 1 && ch != '\n' && i < max)
	buf[i++] = ch;
    if (i < max)
	buf[i] = '\0';
    return rv;
}

int
index_parse(int fd, char *name, char *pathto, char *prefix, char *comment, char *descr, char *maint, char *cats, char *deps)
{
    char line[1024];
    char *cp;
    int i;

    i = readline(fd, line, 1024);
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
    (void)copy_to_sep(deps, cp, '|');
    /* We're not actually interested in any of the other fields */
    return 0;
}

int
index_get(char *fname, PkgNodePtr papa)
{
    int i, fd;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "Unable to open index file `%s' for reading.\n", fname);
	i = -1;
    }
    else
	i = index_read(fd, papa);
    close(fd);
    return i;
}

int
index_read(int fd, PkgNodePtr papa)
{
    char name[127], pathto[255], prefix[255], comment[255], descr[127], maint[127], cats[511], deps[511];

    while (index_parse(fd, name, pathto, prefix, comment, descr, maint, cats, deps) != EOF) {
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
    top->next = top->kids = NULL;
    top->name = "Package Selection";
    top->type = PLACE;
    top->desc = fetch_desc(top->name);
    top->data = NULL;

    plist->next = plist->kids = NULL;
    plist->name = "Package Targets";
    plist->type = PLACE;
    plist->desc = fetch_desc(plist->name);
    plist->data = NULL;
}

void
index_entry_free(IndexEntryPtr top)
{
    safe_free(top->name);
    safe_free(top->path);
    safe_free(top->prefix);
    safe_free(top->comment);
    safe_free(top->descrfile);
    safe_free(top->maintainer);
    free(top);
}

void
index_node_free(PkgNodePtr top, PkgNodePtr plist)
{
    PkgNodePtr tmp;

    tmp = plist;
    while (tmp) {
	PkgNodePtr tmp2 = tmp->next;
	    
	safe_free(tmp);
	tmp = tmp2;
    }

    for (tmp = top; tmp; tmp = tmp->next) {
	free(tmp->name);
	if (tmp->type == PACKAGE && tmp->data)
	    index_entry_free((IndexEntryPtr)tmp->data);
	if (tmp->kids)
	    index_node_free(tmp->kids, NULL);
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

/*
 * No, we don't free n because someone else is still pointing at it.
 * It's just clone linked from another location, which we're adjusting.
 */
void
index_delete(PkgNodePtr n)
{
    if (n->next)
	*n = *(n->next);
    else /* Kludgy end sentinal */
	n->name = NULL;
}

PkgNodePtr
index_search(PkgNodePtr top, char *str, PkgNodePtr *tp)
{
    PkgNodePtr p, sp;

    for (p = top->kids; p && p->name; p = p->next) {
	/* Subtract out the All category from searches */
	if (!strcmp(p->name, "All"))
	    continue;

	/* If tp == NULL, we're looking for an exact package match */
	if (!tp && !strcmp(p->name, str))
	    return p;

	/* If tp, we're looking for both a package and a pointer to the place it's in */
	if (tp && strstr(p->name, str)) {
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

/* Work function for seeing if name x is in result string y */
static Boolean
is_selected_in(char *name, char *result)
{
    Boolean ret = FALSE;

    while (*result) {
	char *cp;

	cp = index(result, '\n');
	if (!cp) {
	    ret = !strcmp(name, result);
	    break;
	}
	else {
	    ret = !strncmp(name, result, cp - result - 1);
	    if (ret)
		break;
	}
	result = cp + 1;
    }
    return ret;
}

int
index_menu(PkgNodePtr top, PkgNodePtr plist, int *pos, int *scroll)
{
    int n, rval, maxname;
    int curr, max;
    PkgNodePtr sp, kp;
    char **nitems;
    char result[4096];
    Boolean hasPackages;

    hasPackages = FALSE;
    nitems = NULL;

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
	dialog_clear();
	msgConfirm("The %s menu is empty.", top->name);
	return RET_DONE;
    }

    dialog_clear();
    while (1) {
	n = 0;
	curr = max = 0;
	kp = top->kids;
	if (!hasPackages && kp && kp->name && plist) {
	    nitems = item_add_pair(nitems, "UP", "<RETURN TO PREVIOUS MENU>", &curr, &max);
	    ++n;
	}
	while (kp && kp->name) {
	    /* Brutally adjust description to fit in menu */
	    if (strlen(kp->desc) > (_MAX_DESC - maxname))
		kp->desc[_MAX_DESC - maxname] = '\0';
	    nitems = item_add_pair(nitems, kp->name, kp->desc, &curr, &max);
	    if (hasPackages) {
		if (kp->type == PACKAGE && plist)
		    nitems = item_add(nitems, index_search(plist, kp->name, NULL) ? "ON" : "OFF", &curr, &max);
		else
		    nitems = item_add(nitems, "OFF", &curr, &max);
	    }
	    ++n;
	    kp = kp->next;
	}
	nitems = item_add(nitems, NULL, &curr, &max);

	if (hasPackages)
	    rval = dialog_checklist(top->name, top->desc, -1, -1, n > MAX_MENU ? MAX_MENU : n, n,
				    (unsigned char **)nitems, result);
	else	/* It's a categories menu */
	    rval = dialog_menu(top->name, top->desc, -1, -1, n > MAX_MENU ? MAX_MENU : n, n,
			       (unsigned char **)nitems, result, pos, scroll);
	if (!rval && plist && strcmp(result, "UP")) {
	    for (kp = top->kids; kp; kp = kp->next) {
		if (kp->type == PACKAGE) {
		    sp = index_search(plist, kp->name, NULL);
		    if (is_selected_in(kp->name, result)) {
			if (!sp) {
			    PkgNodePtr np = (PkgNodePtr)safe_malloc(sizeof(PkgNode));

			    *np = *kp;
			    np->next = plist->kids;
			    plist->kids = np;
			    standout();
			    mvprintw(23, 0, "Selected packages were added to selection list\n", kp->name);
			    standend();
			    refresh();
			}
		    }
		    else if (sp) {
			standout();
			mvprintw(23, 0, "Deleting unselected packages from selection list\n", kp->name);
			standend();
			refresh();
			index_delete(sp);
		    }
		}
		else if (!strcmp(kp->name, result)) {	/* Not a package, must be a directory */
		    int p, s;
		    
		    p = s = 0;
		    index_menu(kp, plist, &p, &s);
		}
	    }
	}
	else if (rval == -1 && plist) {
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
		else {
		    msgConfirm("Search string: %s yielded no hits.", cp);
		}
	    }
	}
	else {
	    dialog_clear();
	    items_free(nitems, &curr, &max);
	    return rval ? RET_FAIL : RET_SUCCESS;
	}
    }
}

int
index_extract(Device *dev, PkgNodePtr top, PkgNodePtr plist)
{
    PkgNodePtr tmp;
    int status = RET_SUCCESS;

    for (tmp = plist->kids; tmp; tmp = tmp->next)
	status = index_extract_one(dev, top, tmp);
    return status;
}

static int
index_extract_one(Device *dev, PkgNodePtr top, PkgNodePtr who)
{
    int status = RET_SUCCESS;
    PkgNodePtr tmp2;

    if (((IndexEntryPtr)who->data)->deps) {
	char t[1024], *cp, *cp2;

	strcpy(t, ((IndexEntryPtr)who->data)->deps);
	cp = t;
	while (cp) {
	    if ((cp2 = index(cp, ' ')) != NULL)
		*cp2 = '\0';
	    if (index_search(top, cp, &tmp2)) {
		status = index_extract_one(dev, top, tmp2);
		if (status != RET_SUCCESS) {
		    msgDebug("Loading of dependant package %s failed\n", cp);
		    break;
		}
		status = package_extract(dev, cp);
		if (status != RET_SUCCESS)
		    break;
		if (cp2)
		    cp = cp2 + 1;
		else
		    cp = NULL;
	    }
	}
    }
    /* Done with the deps?  Load the real m'coy */
    if (status == RET_SUCCESS)
	status = package_extract(dev, who->name);
    return status;
}
