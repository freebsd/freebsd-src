/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vfs_init.c	8.3 (Berkeley) 1/4/94
 * $FreeBSD$
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/malloc.h>


MALLOC_DEFINE(M_VNODE, "vnodes", "Dynamically allocated vnodes");

/*
 * The highest defined VFS number.
 */
int maxvfsconf = VFS_GENERIC + 1;

/*
 * Single-linked list of configured VFSes.
 * New entries are added/deleted by vfs_register()/vfs_unregister()
 */
struct vfsconf *vfsconf;

/*
 * vfs_init.c
 *
 * Allocate and fill in operations vectors.
 *
 * An undocumented feature of this approach to defining operations is that
 * there can be multiple entries in vfs_opv_descs for the same operations
 * vector. This allows third parties to extend the set of operations
 * supported by another layer in a binary compatibile way. For example,
 * assume that NFS needed to be modified to support Ficus. NFS has an entry
 * (probably nfs_vnopdeop_decls) declaring all the operations NFS supports by
 * default. Ficus could add another entry (ficus_nfs_vnodeop_decl_entensions)
 * listing those new operations Ficus adds to NFS, all without modifying the
 * NFS code. (Of couse, the OTW NFS protocol still needs to be munged, but
 * that is a(whole)nother story.) This is a feature.
 */

/* Table of known vnodeop vectors (list of VFS vnode vectors) */
static const struct vnodeopv_desc **vnodeopv_descs;
static int vnodeopv_num;

/* Table of known descs (list of vnode op handlers "vop_access_desc") */
static struct vnodeop_desc **vfs_op_descs;
/* Reference counts for vfs_op_descs */
static int *vfs_op_desc_refs;
/* Number of descriptions */
static int num_op_descs;
/* Number of entries in each description */
static int vfs_opv_numops = 64;

/* Allow this number to be tuned at boot */
TUNABLE_INT("vfs.opv_numops", &vfs_opv_numops);
SYSCTL_INT(_vfs, OID_AUTO, opv_numops, CTLFLAG_RD, &vfs_opv_numops,
	0, "Maximum number of operations in vop_t vector");

static int int_cmp(const void *a, const void *b);

static int
int_cmp(const void *a, const void *b)
{
	return(*(const int *)a - *(const int *)b);
}

/*
 * Recalculate the operations vector/description (those parts of it that can
 * be recalculated, that is.)
 * Always allocate operations vector large enough to hold vfs_opv_numops
 * entries. The vector is never freed or deallocated once it is initialized,
 * so that vnodes might safely reference it through their v_op pointer without
 * vector changing suddenly from under them.
 */
static void
vfs_opv_recalc(void)
{
	int i, j, k;
	int *vfs_op_offsets;
	vop_t ***opv_desc_vector_p;
	vop_t **opv_desc_vector;
	struct vnodeopv_entry_desc *opve_descp;
	const struct vnodeopv_desc *opv;

	if (vfs_op_descs == NULL)
		panic("vfs_opv_recalc called with null vfs_op_descs");

	/*
	 * Allocate and initialize temporary array to store
	 * offsets. Sort it to put all uninitialized entries
	 * first and to make holes in existing offset sequence
	 * detectable.
	 */
	MALLOC(vfs_op_offsets, int *,
		num_op_descs * sizeof(int), M_TEMP, 0);
	if (vfs_op_offsets == NULL)
		panic("vfs_opv_recalc: no memory");
	for (i = 0; i < num_op_descs; i++)
		vfs_op_offsets[i] = vfs_op_descs[i]->vdesc_offset;
	qsort(vfs_op_offsets, num_op_descs, sizeof(int), int_cmp);

	/*
	 * Run through and make sure all known descs have an offset.
	 * Use vfs_op_offsets to locate holes in offset sequence and
	 * reuse them.
	 * vop_default_desc is hardwired at offset 1, and offset 0
	 * is a panic sanity check.
	 */
	j = 1; k = 1;
	for (i = 0; i < num_op_descs; i++) {
		if (vfs_op_descs[i]->vdesc_offset != 0)
			continue;
		/*
		 * Look at two adjacent entries vfs_op_offsets[j - 1] and
		 * vfs_op_offsets[j] and see if we can fit a new offset
		 * number in between. If not, look at the next pair until
		 * hole is found or the end of the vfs_op_offsets vector is
		 * reached. j has been initialized to 1 above so that
		 * referencing (j-1)-th element is safe and the loop will
		 * never execute if num_op_descs is 1. For each new value s
		 * of i the j loop pick up from where previous iteration has
		 * left off. When the last hole has been consumed or if no
		 * hole has been found, we will start allocating new numbers
		 * starting from the biggest already available offset + 1.
		 */
		for (; j < num_op_descs; j++) {
			if (vfs_op_offsets[j - 1] < k && vfs_op_offsets[j] > k)
				break;
			k = vfs_op_offsets[j] + 1;
		}
		vfs_op_descs[i]->vdesc_offset = k++;
	}
	FREE(vfs_op_offsets, M_TEMP);

	/* Panic if new vops will cause vector overflow */
	if (k > vfs_opv_numops)
		panic("VFS: Ran out of vop_t vector entries. %d entries required, only %d available.\n", k, vfs_opv_numops);

	/*
	 * Allocate and fill in the vectors
	 */
	for (i = 0; i < vnodeopv_num; i++) {
		opv = vnodeopv_descs[i];
		opv_desc_vector_p = opv->opv_desc_vector_p;
		if (*opv_desc_vector_p == NULL)
			MALLOC(*opv_desc_vector_p, vop_t **,
				vfs_opv_numops * sizeof(vop_t *), M_VNODE,
				M_ZERO);

		/* Fill in, with slot 0 being to return EOPNOTSUPP */
		opv_desc_vector = *opv_desc_vector_p;
		opv_desc_vector[0] = (vop_t *)vop_eopnotsupp;
		for (j = 0; opv->opv_desc_ops[j].opve_op; j++) {
			opve_descp = &(opv->opv_desc_ops[j]);
			opv_desc_vector[opve_descp->opve_op->vdesc_offset] =
				opve_descp->opve_impl;
		}

		/* Replace unfilled routines with their default (slot 1). */
		opv_desc_vector = *(opv->opv_desc_vector_p);
		if (opv_desc_vector[1] == NULL)
			panic("vfs_opv_recalc: vector without a default.");
		for (j = 0; j < vfs_opv_numops; j++)
			if (opv_desc_vector[j] == NULL)
				opv_desc_vector[j] = opv_desc_vector[1];
	}
}

/* Add a set of vnode operations (a description) to the table above. */
void
vfs_add_vnodeops(const void *data)
{
	const struct vnodeopv_desc *opv;
	const struct vnodeopv_desc **newopv;
	struct vnodeop_desc **newop;
	int *newref;
	vop_t **opv_desc_vector;
	struct vnodeop_desc *desc;
	int i, j;

	opv = (const struct vnodeopv_desc *)data;
	MALLOC(newopv, const struct vnodeopv_desc **,
	       (vnodeopv_num + 1) * sizeof(*newopv), M_VNODE, 0);
	if (vnodeopv_descs) {
		bcopy(vnodeopv_descs, newopv, vnodeopv_num * sizeof(*newopv));
		FREE(vnodeopv_descs, M_VNODE);
	}
	newopv[vnodeopv_num] = opv;
	vnodeopv_descs = newopv;
	vnodeopv_num++;

	/* See if we have turned up a new vnode op desc */
	opv_desc_vector = *(opv->opv_desc_vector_p);
	for (i = 0; (desc = opv->opv_desc_ops[i].opve_op); i++) {
		for (j = 0; j < num_op_descs; j++) {
			if (desc == vfs_op_descs[j]) {
				/* found it, increase reference count */
				vfs_op_desc_refs[j]++;
				break;
			}
		}
		if (j == num_op_descs) {
			/* not found, new entry */
			MALLOC(newop, struct vnodeop_desc **,
			       (num_op_descs + 1) * sizeof(*newop),
			       M_VNODE, 0);
			/* new reference count (for unload) */
			MALLOC(newref, int *,
				(num_op_descs + 1) * sizeof(*newref),
				M_VNODE, 0);
			if (vfs_op_descs) {
				bcopy(vfs_op_descs, newop,
					num_op_descs * sizeof(*newop));
				FREE(vfs_op_descs, M_VNODE);
			}
			if (vfs_op_desc_refs) {
				bcopy(vfs_op_desc_refs, newref,
					num_op_descs * sizeof(*newref));
				FREE(vfs_op_desc_refs, M_VNODE);
			}
			newop[num_op_descs] = desc;
			newref[num_op_descs] = 1;
			vfs_op_descs = newop;
			vfs_op_desc_refs = newref;
			num_op_descs++;
		}
	}
	vfs_opv_recalc();
}

/* Remove a vnode type from the vnode description table above. */
void
vfs_rm_vnodeops(const void *data)
{
	const struct vnodeopv_desc *opv;
	const struct vnodeopv_desc **newopv;
	struct vnodeop_desc **newop;
	int *newref;
	vop_t **opv_desc_vector;
	struct vnodeop_desc *desc;
	int i, j, k;

	opv = (const struct vnodeopv_desc *)data;
	/* Lower ref counts on descs in the table and release if zero */
	for (i = 0; (desc = opv->opv_desc_ops[i].opve_op); i++) {
		for (j = 0; j < num_op_descs; j++) {
			if (desc == vfs_op_descs[j]) {
				/* found it, decrease reference count */
				vfs_op_desc_refs[j]--;
				break;
			}
		}
		for (j = 0; j < num_op_descs; j++) {
			if (vfs_op_desc_refs[j] > 0)
				continue;
			if (vfs_op_desc_refs[j] < 0)
				panic("vfs_remove_vnodeops: negative refcnt");
			/* Entry is going away - replace it with defaultop */
			for (k = 0; k < vnodeopv_num; k++) {
				opv_desc_vector = 
					*(vnodeopv_descs[k]->opv_desc_vector_p);
				if (opv_desc_vector != NULL)
					opv_desc_vector[desc->vdesc_offset] =
						opv_desc_vector[1];
			}
			MALLOC(newop, struct vnodeop_desc **,
			       (num_op_descs - 1) * sizeof(*newop),
			       M_VNODE, 0);
			/* new reference count (for unload) */
			MALLOC(newref, int *,
				(num_op_descs - 1) * sizeof(*newref),
				M_VNODE, 0);
			for (k = j; k < (num_op_descs - 1); k++) {
				vfs_op_descs[k] = vfs_op_descs[k + 1];
				vfs_op_desc_refs[k] = vfs_op_desc_refs[k + 1];
			}
			bcopy(vfs_op_descs, newop,
				(num_op_descs - 1) * sizeof(*newop));
			bcopy(vfs_op_desc_refs, newref,
				(num_op_descs - 1) * sizeof(*newref));
			FREE(vfs_op_descs, M_VNODE);
			FREE(vfs_op_desc_refs, M_VNODE);
			vfs_op_descs = newop;
			vfs_op_desc_refs = newref;
			num_op_descs--;
		}
	}

	for (i = 0; i < vnodeopv_num; i++) {
		if (vnodeopv_descs[i] == opv) {
			for (j = i; j < (vnodeopv_num - 1); j++)
				vnodeopv_descs[j] = vnodeopv_descs[j + 1];
			break;
		}
	}
	if (i == vnodeopv_num)
		panic("vfs_remove_vnodeops: opv not found");
	opv_desc_vector = *(opv->opv_desc_vector_p);
	if (opv_desc_vector != NULL)
		FREE(opv_desc_vector, M_VNODE);
	MALLOC(newopv, const struct vnodeopv_desc **,
	       (vnodeopv_num - 1) * sizeof(*newopv), M_VNODE, 0);
	bcopy(vnodeopv_descs, newopv, (vnodeopv_num - 1) * sizeof(*newopv));
	FREE(vnodeopv_descs, M_VNODE);
	vnodeopv_descs = newopv;
	vnodeopv_num--;

	vfs_opv_recalc();
}

/*
 * Routines having to do with the management of the vnode table.
 */
struct vattr va_null;

/*
 * Initialize the vnode structures and initialize each filesystem type.
 */
/* ARGSUSED*/
static void
vfsinit(void *dummy)
{

	vattr_null(&va_null);
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_FIRST, vfsinit, NULL)

/* Register a new filesystem type in the global table */
int
vfs_register(struct vfsconf *vfc)
{
	struct sysctl_oid *oidp;
	struct vfsconf *vfsp;

	vfsp = NULL;
	if (vfsconf)
		for (vfsp = vfsconf; vfsp->vfc_next; vfsp = vfsp->vfc_next)
			if (strcmp(vfc->vfc_name, vfsp->vfc_name) == 0)
				return EEXIST;

	vfc->vfc_typenum = maxvfsconf++;
	if (vfsp)
		vfsp->vfc_next = vfc;
	else
		vfsconf = vfc;
	vfc->vfc_next = NULL;

	/*
	 * If this filesystem has a sysctl node under vfs
	 * (i.e. vfs.xxfs), then change the oid number of that node to 
	 * match the filesystem's type number.  This allows user code
	 * which uses the type number to read sysctl variables defined
	 * by the filesystem to continue working. Since the oids are
	 * in a sorted list, we need to make sure the order is
	 * preserved by re-registering the oid after modifying its
	 * number.
	 */
	SLIST_FOREACH(oidp, &sysctl__vfs_children, oid_link)
		if (strcmp(oidp->oid_name, vfc->vfc_name) == 0) {
			sysctl_unregister_oid(oidp);
			oidp->oid_number = vfc->vfc_typenum;
			sysctl_register_oid(oidp);
		}

	/*
	 * Call init function for this VFS...
	 */
	(*(vfc->vfc_vfsops->vfs_init))(vfc);

	return 0;
}


/* Remove registration of a filesystem type */
int
vfs_unregister(struct vfsconf *vfc)
{
	struct vfsconf *vfsp, *prev_vfsp;
	int error, i, maxtypenum;

	i = vfc->vfc_typenum;

	prev_vfsp = NULL;
	for (vfsp = vfsconf; vfsp;
			prev_vfsp = vfsp, vfsp = vfsp->vfc_next) {
		if (!strcmp(vfc->vfc_name, vfsp->vfc_name))
			break;
	}
	if (vfsp == NULL)
		return EINVAL;
	if (vfsp->vfc_refcount)
		return EBUSY;
	if (vfc->vfc_vfsops->vfs_uninit != NULL) {
		error = (*vfc->vfc_vfsops->vfs_uninit)(vfsp);
		if (error)
			return (error);
	}
	if (prev_vfsp)
		prev_vfsp->vfc_next = vfsp->vfc_next;
	else
		vfsconf = vfsp->vfc_next;
	maxtypenum = VFS_GENERIC;
	for (vfsp = vfsconf; vfsp != NULL; vfsp = vfsp->vfc_next)
		if (maxtypenum < vfsp->vfc_typenum)
			maxtypenum = vfsp->vfc_typenum;
	maxvfsconf = maxtypenum + 1;
	return 0;
}

/*
 * Standard kernel module handling code for filesystem modules.
 * Referenced from VFS_SET().
 */
int
vfs_modevent(module_t mod, int type, void *data)
{
	struct vfsconf *vfc;
	int error = 0;

	vfc = (struct vfsconf *)data;

	switch (type) {
	case MOD_LOAD:
		if (vfc)
			error = vfs_register(vfc);
		break;

	case MOD_UNLOAD:
		if (vfc)
			error = vfs_unregister(vfc);
		break;
	default:	/* including MOD_SHUTDOWN */
		break;
	}
	return (error);
}
