/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include "kernel_interface.h"
#include "kernel_plm.h"
#include "lomacfs.h"
#include "policy_plm.h"

MALLOC_DEFINE(M_LOMACPLM, "LOMAC_PLM", "LOMAC PLM nodes and strings");
char *strsep(register char **stringp, register const char *delim);

/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
char *
strsep(stringp, delim)
	register char **stringp;
	register const char *delim;
{
	register char *s;
	register const char *spanp;
	register int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

struct lomac_node_entry lomac_node_entry_root = {
	SLIST_HEAD_INITIALIZER(lomac_node_entry),
	{ NULL },
	LN_HIGHEST_LEVEL | LN_INHERIT_HIGH,
	"/"
};

static struct lomac_node_entry *
lomac_plm_subtree_find_cnp(struct lomac_node_entry *root,
    struct componentname *cnp) {
	char *nameptr = cnp->cn_nameptr;
	struct lomac_node_entry *lne;
	int len = cnp->cn_namelen;
	
	SLIST_FOREACH(lne, &root->ln_children, ln_chain)
		if (strlen(lne->ln_name) == len &&
		    bcmp(lne->ln_name, nameptr, len) == 0)
			break;

	return (lne);
}

static struct lomac_node_entry *
lomac_plm_subtree_find(struct lomac_node_entry *root, const char *name) {
	struct lomac_node_entry *lne;
	
	SLIST_FOREACH(lne, &root->ln_children, ln_chain)
		if (strcmp(name, lne->ln_name) == 0)
			break;

	return (lne);
}


/*
 * This is called from inside getnewvnode() before the vnode is in use.
 */
void
lomac_plm_init_lomacfs_vnode(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp, lattr_t *subjlattr) {
	struct lomac_node *ln = VTOLOMAC(vp);
	struct lomac_node_entry *mlne = NULL;

	/*
	 * Only "/" has no parent, so inherit directly from our PLM root.
	 */
	if (dvp == NULL) {
		ln->ln_flags = lomac_node_entry_root.ln_flags;
		ln->ln_entry = ln->ln_underpolicy = &lomac_node_entry_root;
	} else {
		struct lomac_node *dln = VTOLOMAC(dvp);
		struct lomac_node_entry *dlne = dln->ln_entry;
		int fixup_inherit = 0;

		/*
		 * If we have no directory-specific entry, we inherit
		 * directly from the lomac_node's previously-inherited
		 * flags implicitly, otherwise we inherit explicitly
		 * from the corresponding lomac_node_entry.
		 */
		if (dlne == NULL) {
			ln->ln_flags = dln->ln_flags & LN_INHERIT_MASK;
			fixup_inherit = 1;
			ln->ln_underpolicy = dln->ln_underpolicy;
			ln->ln_entry = NULL;
		} else if ((mlne = lomac_plm_subtree_find_cnp(dlne, cnp)) ==
		    NULL) {
			ln->ln_flags = dlne->ln_flags & LN_INHERIT_MASK;
			fixup_inherit = 2;
			ln->ln_underpolicy = dlne;
			ln->ln_entry = NULL;
		} else {
			ln->ln_entry = ln->ln_underpolicy = mlne;
		}
		if (fixup_inherit) {
			switch (ln->ln_flags) {
			case LN_INHERIT_LOW:
				ln->ln_flags |= LN_LOWEST_LEVEL;
				break;
			case LN_INHERIT_SUBJ:
				if (subjlattr->level == LOMAC_HIGHEST_LEVEL)
					ln->ln_flags |= LN_HIGHEST_LEVEL;
				else {
					ln->ln_flags &= ~LN_INHERIT_MASK;
					ln->ln_flags |= LN_INHERIT_LOW |
					    LN_LOWEST_LEVEL;
				}
				break;
			case LN_INHERIT_HIGH:
				ln->ln_flags |= LN_HIGHEST_LEVEL;
				break;
			}
			if (fixup_inherit == 2)
				ln->ln_flags |=
				    (dlne->ln_flags & LN_CHILD_ATTR_MASK) >>
				    LN_CHILD_ATTR_SHIFT;
		} else {
			/* this is the only case where mlne != NULL */
			ln->ln_flags &= ~(LN_INHERIT_MASK | LN_ATTR_MASK);
			ln->ln_flags |= mlne->ln_flags &
			    (LN_INHERIT_MASK | LN_ATTR_MASK);
			if ((mlne->ln_flags & LN_LEVEL_MASK) ==
			    LN_SUBJ_LEVEL) {
				if (subjlattr->level == LOMAC_HIGHEST_LEVEL)
					ln->ln_flags |= LN_HIGHEST_LEVEL;
				else
					ln->ln_flags |= LN_LOWEST_LEVEL;
			} else
				ln->ln_flags |= mlne->ln_flags & LN_LEVEL_MASK;
		}
	}

	KASSERT(ln->ln_flags & LN_LEVEL_MASK, ("lomac_node has no level"));
	KASSERT(ln->ln_flags & LN_INHERIT_MASK, ("lomac_node has no inherit"));
#ifdef INVARIANTS
	if (mlne != NULL) {
		KASSERT(mlne->ln_flags & LN_LEVEL_MASK,
		    ("lomac_node_entry has no level"));
		KASSERT(mlne->ln_flags & LN_INHERIT_MASK,
		    ("lomac_node_entry has no inherit"));
	}
#endif /* INVARIANTS */
}

static struct lomac_node_entry *
lomac_plm_subtree_new(struct lomac_node_entry *plne, char *name) {
	struct lomac_node_entry *lne;
	static struct lomac_node_entry_head head_init = 
	    SLIST_HEAD_INITIALIZER(lomac_node_entry);

	lne = malloc(sizeof(*lne), M_LOMACPLM, M_WAITOK);
	bcopy(&head_init, &lne->ln_children, sizeof(head_init));
	lne->ln_name = name;
	lne->ln_flags = plne->ln_flags & LN_INHERIT_MASK;
	switch (lne->ln_flags) {
	case LN_INHERIT_LOW:
		lne->ln_flags |= LN_LOWEST_LEVEL;
		break;
	case LN_INHERIT_HIGH:
		lne->ln_flags |= LN_HIGHEST_LEVEL;
		break;
	case LN_INHERIT_SUBJ:
		lne->ln_flags |= LN_SUBJ_LEVEL;
		break;
	}
	SLIST_INSERT_HEAD(&plne->ln_children, lne, ln_chain);
	return (lne);
}

static void
lomac_plm_subtree_free(struct lomac_node_entry *lneself) {
	struct lomac_node_entry_head *head = &lneself->ln_children;
	struct lomac_node_entry *lne;

	while (!SLIST_EMPTY(head)) {
		lne = SLIST_FIRST(head);
		SLIST_REMOVE_HEAD(head, ln_chain);
		lomac_plm_subtree_free(lne);
	}
	free(lneself, M_LOMACPLM);
}

struct string_list {
	SLIST_ENTRY(string_list) entries;
	char string[1];
};
static SLIST_HEAD(, string_list) string_list_head =
    SLIST_HEAD_INITIALIZER(string_list);

static char *
string_list_new(const char *s) {
	struct string_list *sl;

	sl = malloc(sizeof(*sl) + strlen(s), M_LOMACPLM, M_WAITOK);
	strcpy(sl->string, s);
	SLIST_INSERT_HEAD(&string_list_head, sl, entries);

	return (sl->string);
}

static void
lomac_plm_uninitialize(void) {
	struct lomac_node_entry_head *head = &lomac_node_entry_root.ln_children;
	struct lomac_node_entry *lne;
	struct string_list *sl;

	while (!SLIST_EMPTY(head)) {
		lne = SLIST_FIRST(head);
		SLIST_REMOVE_HEAD(head, ln_chain);
		lomac_plm_subtree_free(lne);
	}
	while (!SLIST_EMPTY(&string_list_head)) {
		sl = SLIST_FIRST(&string_list_head);
		SLIST_REMOVE_HEAD(&string_list_head, entries);
		free(sl, M_LOMACPLM);
	}
}

static int
lomac_plm_initialize(void) {
	struct lomac_node_entry *plne, *lne;
	plm_rule_t *pr;

	for (pr = plm; pr->path != NULL; pr++) {
		char *path;
		char *comp;
		int depth;
		
		if (*pr->path == '\0') {
			printf("lomac_plm: invalid path \"%s\"\n", pr->path);
			return (EINVAL);
		}
		path = string_list_new(pr->path);
		lne = &lomac_node_entry_root;
		depth = 0;
		for (;; depth++) {
			plne = lne;
			comp = strsep(&path, "/");
			if (comp == NULL)
				break;
			if (depth == 0) {	/* special case: beginning / */
				if (*comp == '\0')
					continue;
				else {
					printf("lomac_plm: not absolute path "
					    "\"%s\"\n", pr->path);
					return (EINVAL);
				}
			} else if (depth == 1) {	/* special case: "/" */
				if (*comp == '\0' && strsep(&path, "/") == NULL)
					break;
			}
			if (*comp == '\0' ||
			    strcmp(comp, ".") == 0 ||
			    strcmp(comp, "..") == 0) {
				printf("lomac_plm: empty path component in "
				    "\"%s\"\n", pr->path);
				return (EINVAL);
			}
			lne = lomac_plm_subtree_find(plne, comp);
			if (lne == NULL) {
				lne = lomac_plm_subtree_new(plne, comp);
				lne->ln_path = plne->ln_path;
			}
		}
		lne->ln_path = pr->path;
		if (pr->flags == PLM_NOFLAGS)
			lne->ln_flags &= ~LN_LEVEL_MASK;
		else
			lne->ln_flags &= ~LN_INHERIT_MASK;
		lne->ln_flags |= 
		    plm_levelflags_to_node_flags[pr->level][pr->flags];
		if (pr->flags == PLM_NOFLAGS)
			lne->ln_flags |= pr->attr;
		else
			lne->ln_flags |= (pr->attr & LN_ATTR_MASK)
			    << LN_CHILD_ATTR_SHIFT;
	}
	return (0);
}

int lomac_plm_initialized = 0;

static int
lomac_plm_modevent(module_t module, int event, void *unused) {
	int error = 0;

	switch ((enum modeventtype)event) {
	case MOD_LOAD:
		error = lomac_plm_initialize();
		if (error == 0)
			lomac_plm_initialized = 1;
		break;
	case MOD_UNLOAD:
		lomac_plm_uninitialize();
	case MOD_SHUTDOWN:
		break;
	}
	return (error);
}

static moduledata_t lomac_plm_moduledata = {
	"lomac_plm",
	&lomac_plm_modevent,
	NULL
};
DECLARE_MODULE(lomac_plm, lomac_plm_moduledata, SI_SUB_VFS, SI_ORDER_ANY);
MODULE_VERSION(lomac_plm, 1);
