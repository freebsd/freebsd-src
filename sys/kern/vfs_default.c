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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>

int vop_notsupp __P((struct vop_generic_args *ap));
static int vop_nostrategy __P((struct vop_strategy_args *));

/*
 * This vnode table stores what we want to do if the filesystem doesn't
 * implement a particular VOP.
 *
 * If there is no specific entry here, we will return EOPNOTSUPP.
 *
 */

vop_t **default_vnodeop_p;
static struct vnodeopv_entry_desc default_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_notsupp },
	{ &vop_abortop_desc,		(vop_t *) nullop },
	{ &vop_bwrite_desc,		(vop_t *) vn_bwrite },
	{ &vop_lease_desc,		(vop_t *) lease_check },
	{ &vop_poll_desc,		(vop_t *) vop_nopoll },
	{ &vop_revoke_desc,		(vop_t *) vop_revoke },
	{ &vop_strategy_desc,		(vop_t *) vop_nostrategy },
	{ NULL, NULL }
};

static struct vnodeopv_desc default_vnodeop_opv_desc =
        { &default_vnodeop_p, default_vnodeop_entries };

VNODEOP_SET(default_vnodeop_opv_desc);

int
vop_notsupp(struct vop_generic_args *ap)
{
	/*
	printf("vn_notsupp[%s]\n", ap->a_desc->vdesc_name);
	*/

	return (EOPNOTSUPP);
}

int
vn_defaultop(struct vop_generic_args *ap)
{

	return (VOCALL(default_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}


static int
vop_nostrategy (struct vop_strategy_args *ap)
{
	printf("No strategy for buffer at %p\n", ap->a_bp);
	vprint("", ap->a_bp->b_vp);
	ap->a_bp->b_flags |= B_ERROR;
	ap->a_bp->b_error = EOPNOTSUPP;
	biodone(ap->a_bp);
	return (EOPNOTSUPP);
}
