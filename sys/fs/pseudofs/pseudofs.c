/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/pseudofs/pseudofs_internal.h>

SYSCTL_NODE(_vfs, OID_AUTO, pfs, CTLFLAG_RW, 0,
    "pseudofs");

/*
 * Mount a pseudofs instance
 */
int
pfs_mount(struct pfs_info *pi, struct mount *mp, char *path, caddr_t data,
	  struct nameidata *ndp, struct thread *td)
{
	struct statfs *sbp;
  
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);
	
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t)pi;
	vfs_getnewfsid(mp);

	sbp = &mp->mnt_stat;
	bcopy(pi->pi_name, sbp->f_mntfromname, sizeof pi->pi_name);
	sbp->f_bsize = PAGE_SIZE;
	sbp->f_iosize = PAGE_SIZE;
	sbp->f_blocks = 1;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;
	sbp->f_ffree = 0;

	return (0);
}

/*
 * Unmount a pseudofs instance
 */
int
pfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct pfs_info *pi;
	int error;

	pi = (struct pfs_info *)mp->mnt_data;

	/* XXX do stuff with pi... */
	
	error = vflush(mp, 0, (mntflags & MNT_FORCE) ?  FORCECLOSE : 0);
	return (error);
}

/*
 * Return a root vnode
 */
int
pfs_root(struct mount *mp, struct vnode **vpp)
{
	struct pfs_info *pi;

	pi = (struct pfs_info *)mp->mnt_data;
	return pfs_vncache_alloc(mp, vpp, pi->pi_root, NO_PID);
}

/*
 * Return filesystem stats
 */
int
pfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	bcopy(&mp->mnt_stat, sbp, sizeof *sbp);
	return (0);
}

/*
 * Initialize a pseudofs instance
 */
int
pfs_init(struct pfs_info *pi, struct vfsconf *vfc)
{
	mtx_init(&pi->pi_mutex, "pseudofs", MTX_DEF);
	pfs_fileno_init(pi);
	if (bootverbose)
		printf("%s registered\n", pi->pi_name);
	return (0);
}

/*
 * Destroy a pseudofs instance
 */
int
pfs_uninit(struct pfs_info *pi, struct vfsconf *vfc)
{
	pfs_fileno_uninit(pi);
	mtx_destroy(&pi->pi_mutex);
	if (bootverbose)
		printf("%s unregistered\n", pi->pi_name);
	return (0);
}

/*
 * Handle load / unload events
 */
static int
pfs_modevent(module_t mod, int evt, void *arg)
{
	switch (evt) {
	case MOD_LOAD:
		pfs_fileno_load();
		pfs_vncache_load();
		break;
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		pfs_vncache_unload();
		pfs_fileno_unload();
		break;
	default:
		printf("pseudofs: unexpected event type %d\n", evt);
		break;
	}
	return 0;
}

/*
 * Module declaration
 */
static moduledata_t pseudofs_data = {
	"pseudofs",
	pfs_modevent,
	NULL
};
DECLARE_MODULE(pseudofs, pseudofs_data, SI_SUB_EXEC, SI_ORDER_FIRST);
MODULE_VERSION(pseudofs, 2);
