/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
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
 * $Id: lomacfs.h,v 1.20 2001/10/17 15:34:29 bfeldman Exp $
 */

#ifndef LOMACFS_H
#define LOMACFS_H

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include "kernel_interface.h"

struct lomac_mount {
	struct vnode *lm_rootvp;	/* singly-ref'd root after mount() */
#define	LM_TOOKROOT		0x0001
	unsigned int lm_flags;
};

/*
 * This is the structure associated with v_data on all LOMACFS vnodes.
 */
struct lomac_node {
	struct vnode *ln_vp;		/* vnode back-pointer */
	struct vnode *ln_lowervp;	/* shadowed vnode (ref'd or NULL) */
#define	LN_LEVEL_MASK		0x0003
#define	LN_LOWEST_LEVEL		0x0001
#define	LN_SUBJ_LEVEL		0x0002	/* placeholder before inheriting */
#define	LN_HIGHEST_LEVEL	0x0003
#define	LN_INHERIT_MASK		0x001c
#define	LN_INHERIT_LOW		0x0004	/* children start with a low level */
#define	LN_INHERIT_HIGH		0x0008	/* children start with a high level */
#define	LN_INHERIT_SUBJ		0x0010	/* children inherit subject's level */
#define	LN_ATTR_MASK		0x01e0
#define	LN_ATTR_LOWWRITE	0x0020	/* lower levels may write to */
#define	LN_ATTR_LOWNOOPEN	0x0040	/* lower levels may not open */
#define	LN_ATTR_NONETDEMOTE	0x0080	/* will not demote on net read */
#define	LN_ATTR_NODEMOTE	0x0100	/* subject won't demote on other read */
	u_int	ln_flags;
	/* What's the last node explicitly specifying policy for this? */
	struct lomac_node_entry *ln_underpolicy;
	/* If non-NULL, this corresponds 1:1 to a specific PLM node entry. */
	struct lomac_node_entry *ln_entry;
#if defined(LOMAC_DEBUG_INCNAME)
	char	ln_name[MAXPATHLEN];	/* final component name */
#endif
};

/*
 * This is the "placeholder" structure initialized from the PLM that
 * holds the level information for all named objects.
 */
struct lomac_node_entry {
	SLIST_HEAD(lomac_node_entry_head, lomac_node_entry) ln_children;
	SLIST_ENTRY(lomac_node_entry) ln_chain;	/* chain of current level */
	/* continuing with the LN_* flags above */
#define	LN_CHILD_ATTR_SHIFT	4	/* lshift from attr -> child attr */
#define	LN_CHILD_ATTR_MASK	0x1e00
#define	LN_CHILD_ATTR_LOWWRITE	0x0200	/* lower levels may write to */
#define	LN_CHILD_ATTR_LOWNOOPEN	0x0400	/* lower levels may not open */
#define	LN_CHILD_ATTR_NONETDEMOTE 0x0800 /* will not demote on net read */
#define	LN_CHILD_ATTR_NODEMOTE	0x1000	/* subject won't demote on other read */
	u_int ln_flags;
	char *ln_name;			/* last component name (to search) */
	const char *ln_path;		/* in "stable" storage */
};

#define	VTOLOMAC(vp)	((struct lomac_node *)(vp)->v_data)
#define	VTOLVP(vp)	VTOLOMAC(vp)->ln_lowervp
#define VFSTOLOMAC(mp)	((struct lomac_mount *)mp->mnt_data)
#define	VISLOMAC(vp)	(vp->v_op == lomacfs_vnodeop_p)

int lomacfs_node_alloc(struct mount *mp, struct componentname *cnp,
    struct vnode *dvp, struct vnode *lowervp, struct vnode **vpp);

MALLOC_DECLARE(M_LOMACFS);
extern vop_t **lomacfs_vnodeop_p;
extern struct lomac_node_entry lomac_node_entry_root;

#endif /* LOMACFS_H */
