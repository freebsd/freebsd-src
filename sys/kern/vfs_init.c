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
 * $Id: vfs_init.c,v 1.29 1997/10/12 20:24:24 phk Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <vm/vm_zone.h>

static void	vfs_op_init __P((void));

static void vfsinit __P((void *));
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_FIRST, vfsinit, NULL)

MALLOC_DEFINE(M_VNODE, "vnodes", "Dynamically allocated vnodes");

/*
 * Sigh, such primitive tools are these...
 */
#if 0
#define DODEBUG(A) A
#else
#define DODEBUG(A)
#endif

struct vfsconf void_vfsconf;

extern struct linker_set vfs_opv_descs_;
#define vfs_opv_descs ((struct vnodeopv_desc **)vfs_opv_descs_.ls_items)

extern struct linker_set vfs_set;

extern struct vnodeop_desc *vfs_op_descs[];
				/* and the operations they perform */

/*
 * Zone for namei
 */
struct vm_zone *namei_zone;

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
 *
 * Without an explicit reserve area, however, you must replace vnode_if.c
 * and vnode_if.h when you do this, or you will be derefrencing of the
 * end of vfs_op_descs[].  This is a flaw in the use of a structure
 * pointer array rather than an agregate to define vfs_op_descs.  So
 * it's not a very dynamic "feature".
 */
void
vfs_opv_init(struct vnodeopv_desc **them)
{
	int i, j, k;
	vop_t ***opv_desc_vector_p;
	vop_t **opv_desc_vector;
	struct vnodeopv_entry_desc *opve_descp;

	/*
	 * Allocate the dynamic vectors and fill them in.
	 */
	for (i=0; them[i]; i++) {
		opv_desc_vector_p = them[i]->opv_desc_vector_p;
		/*
		 * Allocate and init the vector, if it needs it.
		 * Also handle backwards compatibility.
		 */
		if (*opv_desc_vector_p == NULL) {
			/* XXX - shouldn't be M_VNODE */
			MALLOC(*opv_desc_vector_p, vop_t **,
			       vfs_opv_numops * sizeof(vop_t *), M_VNODE,
			       M_WAITOK);
			bzero(*opv_desc_vector_p,
			      vfs_opv_numops * sizeof(vop_t *));
			DODEBUG(printf("vector at %x allocated\n",
			    opv_desc_vector_p));
		}
		opv_desc_vector = *opv_desc_vector_p;
		for (j=0; them[i]->opv_desc_ops[j].opve_op; j++) {
			opve_descp = &(them[i]->opv_desc_ops[j]);

			/*
			 * Sanity check:  is this operation listed
			 * in the list of operations?  We check this
			 * by seeing if its offest is zero.  Since
			 * the default routine should always be listed
			 * first, it should be the only one with a zero
			 * offset.  Any other operation with a zero
			 * offset is probably not listed in
			 * vfs_op_descs, and so is probably an error.
			 *
			 * A panic here means the layer programmer
			 * has committed the all-too common bug
			 * of adding a new operation to the layer's
			 * list of vnode operations but
			 * not adding the operation to the system-wide
			 * list of supported operations.
			 */
			if (opve_descp->opve_op->vdesc_offset == 0 &&
				    opve_descp->opve_op->vdesc_offset !=
				    	VOFFSET(vop_default)) {
				printf("operation %s not listed in %s.\n",
				    opve_descp->opve_op->vdesc_name,
				    "vfs_op_descs");
				panic ("vfs_opv_init: bad operation");
			}
			/*
			 * Fill in this entry.
			 */
			opv_desc_vector[opve_descp->opve_op->vdesc_offset] =
					opve_descp->opve_impl;
		}
	}
	/*
	 * Finally, go back and replace unfilled routines
	 * with their default.  (Sigh, an O(n^3) algorithm.  I
	 * could make it better, but that'd be work, and n is small.)
	 */
	for (i = 0; them[i]; i++) {
		opv_desc_vector = *(them[i]->opv_desc_vector_p);
		/*
		 * Force every operations vector to have a default routine.
		 */
		if (opv_desc_vector[VOFFSET(vop_default)]==NULL) {
			panic("vfs_opv_init: operation vector without default routine.");
		}
		for (k = 0; k<vfs_opv_numops; k++)
			if (opv_desc_vector[k] == NULL)
				opv_desc_vector[k] =
					opv_desc_vector[VOFFSET(vop_default)];
	}
}

/*
 * Initialize known vnode operations vectors.
 */
static void
vfs_op_init()
{
	int i;

	DODEBUG(printf("Vnode_interface_init.\n"));
	DODEBUG(printf ("vfs_opv_numops=%d\n", vfs_opv_numops));
	/*
	 * Set all vnode vectors to a well known value.
	 */
	for (i = 0; vfs_opv_descs[i]; i++)
		*(vfs_opv_descs[i]->opv_desc_vector_p) = NULL;
	/*
	 * assign each op to its offset
	 *
	 * XXX This should not be needed, but is because the per
	 * XXX FS ops tables are not sorted according to the
	 * XXX vnodeop_desc's offset in vfs_op_descs.  This
	 * XXX is the same reason we have to take the hit for
	 * XXX the static inline function calls instead of using
	 * XXX simple macro references.
	 */
	for (i = 0; i < vfs_opv_numops; i++)
		vfs_op_descs[i]->vdesc_offset = i;
}

/*
 * Routines having to do with the management of the vnode table.
 */
extern struct vnodeops dead_vnodeops;
extern struct vnodeops spec_vnodeops;
struct vattr va_null;

/*
 * Initialize the vnode structures and initialize each file system type.
 */
/* ARGSUSED*/
static void
vfsinit(dummy)
	void *dummy;
{
	struct vfsconf **vfc;
	int maxtypenum;

	namei_zone = zinit("NAMEI", MAXPATHLEN, 0, 0, 2);

	/*
	 * Initialize the vnode table
	 */
	vntblinit();
	/*
	 * Initialize the vnode name cache
	 */
	nchinit();
	/*
	 * Build vnode operation vectors.
	 */
	vfs_op_init();
	vfs_opv_init(vfs_opv_descs);   /* finish the job */
	/*
	 * Initialize each file system type.
	 */
	vattr_null(&va_null);
	maxtypenum = 0;
	vfc = (struct vfsconf **)vfs_set.ls_items;
	vfsconf = *vfc;		/* simulate Lite2 vfsconf array */
	while (*vfc) {
		struct vfsconf *vfsp = *vfc;

		vfc++;
		vfsp->vfc_next = *vfc;
		if (maxtypenum <= vfsp->vfc_typenum)
			maxtypenum = vfsp->vfc_typenum + 1;
		(*vfsp->vfc_vfsops->vfs_init)(vfsp);
	}
	/* next vfc_typenum to be used */
	maxvfsconf = maxtypenum;
}

/*
 * kernel related system variables.
 */

/*
 * This goop is here to support a loadable NFS module... grumble...
 *
 * XXX: NFS could plug this into default_vnodeop_p now!
 */
int (*lease_check_hook) __P((struct vop_lease_args *))
     = 0;
void (*lease_updatetime) __P((int))
     = 0;

int
lease_check(ap)
	struct vop_lease_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
		struct ucred *a_cred;
		int a_flag;
	} */ *ap;
{
    if (lease_check_hook)
	return (*lease_check_hook)(ap);
    else
	return 0;
}
