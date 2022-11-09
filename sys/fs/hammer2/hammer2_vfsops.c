/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/vnode.h>

#include <vm/uma.h>

#include "hammer2.h"
#include "hammer2_mount.h"

static int hammer2_unmount(struct mount *, int);
static int hammer2_statfs(struct mount *, struct statfs *);
static void hammer2_update_pmps(hammer2_dev_t *);
static void hammer2_mount_helper(struct mount *, hammer2_pfs_t *);
static void hammer2_unmount_helper(struct mount *, hammer2_pfs_t *,
    hammer2_dev_t *);

MALLOC_DEFINE(M_HAMMER2, "hammer2_mount", "HAMMER2 mount structure");
uma_zone_t zone_buffer_read;
uma_zone_t zone_xops;

/* global list of HAMMER2 */
TAILQ_HEAD(hammer2_mntlist, hammer2_dev); /* <-> hammer2_dev::mntentry */
typedef struct hammer2_mntlist hammer2_mntlist_t;
static hammer2_mntlist_t hammer2_mntlist;

/* global list of PFS */
TAILQ_HEAD(hammer2_pfslist, hammer2_pfs); /* <-> hammer2_pfs::mntentry */
typedef struct hammer2_pfslist hammer2_pfslist_t;
static hammer2_pfslist_t hammer2_pfslist;
static hammer2_pfslist_t hammer2_spmplist;

static struct lock hammer2_mntlk;

static int hammer2_supported_version = HAMMER2_VOL_VERSION_DEFAULT;
int hammer2_cluster_meta_read = 1; /* for physical read-ahead */
int hammer2_cluster_data_read = 4; /* for physical read-ahead */
long hammer2_inode_allocs;
long hammer2_chain_allocs;
long hammer2_dio_allocs;
int hammer2_dio_limit = 256;

SYSCTL_NODE(_vfs, OID_AUTO, hammer2, CTLFLAG_RW, 0, "HAMMER2 filesystem");
SYSCTL_INT(_vfs_hammer2, OID_AUTO, supported_version, CTLFLAG_RD,
    &hammer2_supported_version, 0, "Highest supported HAMMER2 version");
SYSCTL_INT(_vfs_hammer2, OID_AUTO, cluster_meta_read, CTLFLAG_RW,
    &hammer2_cluster_meta_read, 0, "Cluster read count for meta data");
SYSCTL_INT(_vfs_hammer2, OID_AUTO, cluster_data_read, CTLFLAG_RW,
    &hammer2_cluster_data_read, 0, "Cluster read count for user data");
SYSCTL_LONG(_vfs_hammer2, OID_AUTO, inode_allocs, CTLFLAG_RD,
    &hammer2_inode_allocs, 0, "Number of inode allocated");
SYSCTL_LONG(_vfs_hammer2, OID_AUTO, chain_allocs, CTLFLAG_RD,
    &hammer2_chain_allocs, 0, "Number of chain allocated");
SYSCTL_LONG(_vfs_hammer2, OID_AUTO, dio_allocs, CTLFLAG_RD,
    &hammer2_dio_allocs, 0, "Number of dio allocated");
SYSCTL_INT(_vfs_hammer2, OID_AUTO, dio_limit, CTLFLAG_RW,
    &hammer2_dio_limit, 0, "Number of dio to keep for reuse");

static const char *hammer2_opts[] = {
	"export", "from", "hflags", NULL,
};

static int
hammer2_assert_clean(void)
{
	int error = 0;

	KKASSERT(hammer2_inode_allocs == 0);
	if (hammer2_inode_allocs > 0) {
		hprintf("%ld inode left\n", hammer2_inode_allocs);
		error = EINVAL;
	}
	KKASSERT(hammer2_chain_allocs == 0);
	if (hammer2_chain_allocs > 0) {
		hprintf("%ld chain left\n", hammer2_chain_allocs);
		error = EINVAL;
	}
	KKASSERT(hammer2_dio_allocs == 0);
	if (hammer2_dio_allocs > 0) {
		hprintf("%ld dio left\n", hammer2_dio_allocs);
		error = EINVAL;
	}

	return (error);
}

static int
hammer2_init(struct vfsconf *vfsp)
{
	hammer2_assert_clean();

	hammer2_dio_limit = nbuf * 2;
	if (hammer2_dio_limit > 100000)
		hammer2_dio_limit = 100000;

	zone_buffer_read = uma_zcreate("hammer2_buffer_read", 65536,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (zone_buffer_read == NULL) {
		hprintf("failed to create zone_buffer_read\n");
		return (ENOMEM);
	}

	zone_xops = uma_zcreate("hammer2_xops", sizeof(hammer2_xop_t),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (zone_xops == NULL) {
		uma_zdestroy(zone_buffer_read);
		zone_buffer_read = NULL;
		hprintf("failed to create zone_xops\n");
		return (ENOMEM);
	}

	lockinit(&hammer2_mntlk, PVFS, "mntlk", 0, 0);

	TAILQ_INIT(&hammer2_mntlist);
	TAILQ_INIT(&hammer2_pfslist);
	TAILQ_INIT(&hammer2_spmplist);

	return (0);
}

static int
hammer2_uninit(struct vfsconf *vfsp)
{
	lockdestroy(&hammer2_mntlk);

	if (zone_buffer_read) {
		uma_zdestroy(zone_buffer_read);
		zone_buffer_read = NULL;
	}
	if (zone_xops) {
		uma_zdestroy(zone_xops);
		zone_xops = NULL;
	}

	hammer2_assert_clean();

	KKASSERT(TAILQ_EMPTY(&hammer2_mntlist));
	KKASSERT(TAILQ_EMPTY(&hammer2_pfslist));
	KKASSERT(TAILQ_EMPTY(&hammer2_spmplist));

	return (0);
}

/*
 * Core PFS allocator.  Used to allocate or reference the pmp structure
 * for PFS cluster mounts and the spmp structure for media (hmp) structures.
 */
static hammer2_pfs_t *
hammer2_pfsalloc(hammer2_chain_t *chain, const hammer2_inode_data_t *ripdata,
    hammer2_dev_t *force_local)
{
	hammer2_pfs_t *pmp = NULL;
	hammer2_inode_t *iroot;
	int j;

	KASSERT(force_local, ("only local mount allowed"));

	/*
	 * Locate or create the PFS based on the cluster id.  If ripdata
	 * is NULL this is a spmp which is unique and is always allocated.
	 *
	 * If the device is mounted in local mode all PFSs are considered
	 * independent and not part of any cluster.
	 */
	if (ripdata) {
		TAILQ_FOREACH(pmp, &hammer2_pfslist, mntentry) {
			if (force_local != pmp->force_local)
				continue;
			if (force_local == NULL &&
			    bcmp(&pmp->pfs_clid, &ripdata->meta.pfs_clid,
			    sizeof(pmp->pfs_clid)) == 0) {
				break;
			} else if (force_local && pmp->pfs_names[0] &&
			    strcmp(pmp->pfs_names[0], ripdata->filename) == 0) {
				break;
			}
		}
	}

	if (pmp == NULL) {
		pmp = malloc(sizeof(*pmp), M_HAMMER2, M_WAITOK | M_ZERO);
		pmp->force_local = force_local;
		hammer2_spin_init(&pmp->inum_spin, "h2pmp_inosp");
		hammer2_spin_init(&pmp->lru_spin, "h2pmp_lrusp");
		hammer2_mtx_init(&pmp->xop_lock, "h2pmp_xoplk");
		RB_INIT(&pmp->inum_tree);
		TAILQ_INIT(&pmp->lru_list);

		KKASSERT((HAMMER2_IHASH_SIZE & (HAMMER2_IHASH_SIZE - 1)) == 0);
		pmp->ipdep_lists = hashinit(HAMMER2_IHASH_SIZE, M_HAMMER2,
		    &pmp->ipdep_mask);
		KKASSERT(HAMMER2_IHASH_SIZE == pmp->ipdep_mask + 1);

		if (ripdata) {
			pmp->pfs_clid = ripdata->meta.pfs_clid;
			TAILQ_INSERT_TAIL(&hammer2_pfslist, pmp, mntentry);
		} else {
			pmp->flags |= HAMMER2_PMPF_SPMP;
			TAILQ_INSERT_TAIL(&hammer2_spmplist, pmp, mntentry);
		}
	}

	/* Create the PFS's root inode. */
	if ((iroot = pmp->iroot) == NULL) {
		iroot = hammer2_inode_get(pmp, NULL, 1, -1);
		if (ripdata)
			iroot->meta = ripdata->meta;
		pmp->iroot = iroot;
		hammer2_inode_ref(iroot);
		hammer2_inode_unlock(iroot);
	}

	/* Stop here if no chain is passed in. */
	if (chain == NULL)
		goto done;

	/*
	 * When a chain is passed in we must add it to the PFS's root
	 * inode, update pmp->pfs_types[].
	 * When forcing local mode, mark the PFS as a MASTER regardless.
	 */
	hammer2_inode_ref(iroot);
	hammer2_mtx_ex(&iroot->lock);

	j = iroot->cluster.nchains; /* Currently always 0. */
	KASSERT(j == 0, ("nchains %d not 0", j));

	KKASSERT(chain->pmp == NULL);
	chain->pmp = pmp;
	hammer2_chain_ref(chain);
	iroot->cluster.array[j].chain = chain;
	if (force_local)
		pmp->pfs_types[j] = HAMMER2_PFSTYPE_MASTER;
	else
		pmp->pfs_types[j] = ripdata->meta.pfs_type;
	pmp->pfs_names[j] = strdup(ripdata->filename, M_HAMMER2);
	pmp->pfs_hmps[j] = chain->hmp;

	/*
	 * If the PFS is already mounted we must account
	 * for the mount_count here.
	 */
	if (pmp->mp)
		++chain->hmp->mount_count;
	++j;

	iroot->cluster.nchains = j;
	hammer2_assert_cluster(&iroot->cluster);

	hammer2_mtx_unlock(&iroot->lock);
	hammer2_inode_drop(iroot);
done:
	return (pmp);
}

/*
 * Destroy a PFS, typically only occurs after the last mount on a device
 * has gone away.
 */
static void
hammer2_pfsfree(hammer2_pfs_t *pmp)
{
	hammer2_inode_t *iroot;
	hammer2_chain_t *chain;
	int i, chains_still_present = 0;

	KKASSERT(!(pmp->flags & HAMMER2_PMPF_WAITING));

	/* Cleanup our reference on iroot. */
	if (pmp->flags & HAMMER2_PMPF_SPMP)
		TAILQ_REMOVE(&hammer2_spmplist, pmp, mntentry);
	else
		TAILQ_REMOVE(&hammer2_pfslist, pmp, mntentry);

	/* Cleanup chains remaining on LRU list. */
	hammer2_spin_ex(&pmp->lru_spin);
	while ((chain = TAILQ_FIRST(&pmp->lru_list)) != NULL) {
		KKASSERT(chain->flags & HAMMER2_CHAIN_ONLRU);
		atomic_add_int(&pmp->lru_count, -1);
		atomic_clear_int(&chain->flags, HAMMER2_CHAIN_ONLRU);
		TAILQ_REMOVE(&pmp->lru_list, chain, entry);
		hammer2_chain_ref(chain);
		hammer2_spin_unex(&pmp->lru_spin);
		atomic_set_int(&chain->flags, HAMMER2_CHAIN_RELEASE);
		hammer2_chain_drop(chain);
		hammer2_spin_ex(&pmp->lru_spin);
	}
	hammer2_spin_unex(&pmp->lru_spin);

	/* Clean up iroot. */
	iroot = pmp->iroot;
	if (iroot) {
		for (i = 0; i < iroot->cluster.nchains; ++i) {
			chain = iroot->cluster.array[i].chain;
			if (chain && !RB_EMPTY(&chain->core.rbtree)) {
				hprintf("PFS at %s has active chains\n",
				    pmp->mntpt);
				chains_still_present = 1;
			}
		}
		KASSERT(iroot->refs == 1,
		    ("iroot %p refs %d not 1", iroot, iroot->refs));

		hammer2_inode_drop(iroot);
		pmp->iroot = NULL;
	}

	/* Free remaining pmp resources. */
	if (chains_still_present) {
		hprintf("PFS at %s still in use\n", pmp->mntpt);
	} else {
		hammer2_spin_destroy(&pmp->inum_spin);
		hammer2_spin_destroy(&pmp->lru_spin);
		hammer2_mtx_destroy(&pmp->xop_lock);
		hashdestroy(pmp->ipdep_lists, M_HAMMER2, pmp->ipdep_mask);
		free(pmp, M_HAMMER2);
	}
}

/*
 * Remove all references to hmp from the pfs list.  Any PFS which becomes
 * empty is terminated and freed.
 */
static void
hammer2_pfsfree_scan(hammer2_dev_t *hmp, int which)
{
	hammer2_pfs_t *pmp;
	hammer2_inode_t *iroot;
	hammer2_chain_t *rchain;
	struct hammer2_pfslist *wlist;
	int i;

	if (which == 0)
		wlist = &hammer2_pfslist;
	else
		wlist = &hammer2_spmplist;
again:
	TAILQ_FOREACH(pmp, wlist, mntentry) {
		if ((iroot = pmp->iroot) == NULL)
			continue;

		/* Determine if this PFS is affected. */
		for (i = 0; i < HAMMER2_MAXCLUSTER; ++i)
			if (pmp->pfs_hmps[i] == hmp)
				break;
		if (i == HAMMER2_MAXCLUSTER)
			continue;

		/*
		 * Lock the inode and clean out matching chains.
		 * Note that we cannot use hammer2_inode_lock_*()
		 * here because that would attempt to validate the
		 * cluster that we are in the middle of ripping
		 * apart.
		 */
		hammer2_mtx_ex(&iroot->lock);

		/* Remove the chain from matching elements of the PFS. */
		for (i = 0; i < HAMMER2_MAXCLUSTER; ++i) {
			if (pmp->pfs_hmps[i] != hmp)
				continue;
			rchain = iroot->cluster.array[i].chain;
			iroot->cluster.array[i].chain = NULL;
			pmp->pfs_types[i] = HAMMER2_PFSTYPE_NONE;
			if (pmp->pfs_names[i]) {
				free(pmp->pfs_names[i], M_HAMMER2);
				pmp->pfs_names[i] = NULL;
			}
			if (rchain) {
				hammer2_chain_drop(rchain);
				/* focus hint */
				if (iroot->cluster.focus == rchain)
					iroot->cluster.focus = NULL;
			}
			pmp->pfs_hmps[i] = NULL;
		}
		hammer2_mtx_unlock(&iroot->lock);

		/* Cleanup trailing chains.  Gaps may remain. */
		for (i = HAMMER2_MAXCLUSTER - 1; i >= 0; --i)
			if (pmp->pfs_hmps[i])
				break;
		iroot->cluster.nchains = i + 1;

		/* If the PMP has no elements remaining we can destroy it. */
		if (iroot->cluster.nchains == 0) {
			/*
			 * If this was the hmp's spmp, we need to clean
			 * a little more stuff out.
			 */
			if (hmp->spmp == pmp) {
				hmp->spmp = NULL;
				hmp->vchain.pmp = NULL;
			}

			/* Free the pmp and restart the loop. */
			hammer2_pfsfree(pmp);
			goto again;
		}
	}
}

/*
 * Mount or remount HAMMER2 fileystem from physical media.
 */
static int
hammer2_mount(struct mount *mp)
{
	struct vfsoptlist *opts = mp->mnt_optnew;
	struct cdev *dev;
	hammer2_dev_t *hmp = NULL, *hmp_tmp, *force_local;
	hammer2_pfs_t *pmp = NULL, *spmp;
	hammer2_key_t key_next, key_dummy, lhc;
	hammer2_chain_t *chain, *parent;
	const hammer2_inode_data_t *ripdata;
	hammer2_devvp_list_t devvpl;
	hammer2_devvp_t *e, *e_tmp;
	hammer2_chain_t *schain;
	hammer2_xop_head_t *xop;
	char devstr[MNAMELEN] = {0};
	char *fspec = NULL, *mntpt = NULL, *label = NULL;
	int rdonly = (mp->mnt_flag & MNT_RDONLY) != 0;
	int i, hflags, len, error, devvp_found;
	int *hflagsp = NULL;

	if (!rdonly) {
		hprintf("write unsupported\n");
		return (EINVAL);
	}

	/* Retrieve options first. */
	if (vfs_filteropt(opts, hammer2_opts))
		return (EINVAL);

	error = vfs_getopt(opts, "from", (void **)&fspec, &len);
	if (error)
		return (EINVAL);
	if (!fspec || fspec[len - 1] != '\0')
		return (EINVAL);

	error = vfs_getopt(opts, "fspath", (void **)&mntpt, NULL);
	if (error)
		return (EINVAL);

	error = vfs_getopt(opts, "hflags", (void **)&hflagsp, NULL);
	if (error)
		return (EINVAL);
	hflags = *hflagsp;

	if (mp->mnt_flag & MNT_UPDATE)
		return (0);

	bcopy(fspec, devstr, MNAMELEN - 1);
	debug_hprintf("devstr=\"%s\" mntpt=\"%s\"\n", devstr, mntpt);

	/*
	 * Extract device and label, automatically mount @DATA if no label
	 * specified.  Error out if no label or device is specified.  This is
	 * a convenience to match the default label created by newfs_hammer2,
	 * our preference is that a label always be specified.
	 *
	 * NOTE: We allow 'mount @LABEL <blah>'... that is, a mount command
	 *	 that does not specify a device, as long as some HAMMER2 label
	 *	 has already been mounted from that device.  This makes
	 *	 mounting snapshots a lot easier.
	 */
	label = strchr(devstr, '@');
	if (label == NULL || label[1] == 0) {
		/*
		 * DragonFly uses either "BOOT", "ROOT" or "DATA" based
		 * on label[-1].  In FreeBSD, simply use "DATA" by default.
		 */
		label = "DATA";
	} else {
		*label = '\0';
		label++;
	}

	debug_hprintf("device=\"%s\" label=\"%s\" rdonly=%d\n",
	    devstr, label, rdonly);

	/* Initialize all device vnodes. */
	TAILQ_INIT(&devvpl);
	error = hammer2_init_devvp(mp, devstr, &devvpl);
	if (error) {
		hprintf("failed to initialize devvp in %s\n", devstr);
		hammer2_cleanup_devvp(&devvpl);
		return (error);
	}

	/*
	 * Determine if the device has already been mounted.  After this
	 * check hmp will be non-NULL if we are doing the second or more
	 * HAMMER2 mounts from the same device.
	 */
	lockmgr(&hammer2_mntlk, LK_EXCLUSIVE, NULL);
	if (!TAILQ_EMPTY(&devvpl)) {
		/*
		 * Match the device.  Due to the way devfs works,
		 * we may not be able to directly match the vnode pointer,
		 * so also check to see if the underlying device matches.
		 */
		TAILQ_FOREACH(hmp_tmp, &hammer2_mntlist, mntentry) {
			TAILQ_FOREACH(e_tmp, &hmp_tmp->devvp_list, entry) {
				devvp_found = 0;
				TAILQ_FOREACH(e, &devvpl, entry) {
					KKASSERT(e->devvp);
					if (e_tmp->devvp == e->devvp)
						devvp_found = 1;
					if (e_tmp->devvp->v_rdev &&
					    e_tmp->devvp->v_rdev == e->devvp->v_rdev)
						devvp_found = 1;
				}
				if (!devvp_found)
					goto next_hmp;
			}
			hmp = hmp_tmp;
			debug_hprintf("hmp=%p matched\n", hmp);
			break;
next_hmp:
			continue;
		}
	} else {
		/* Match the label to a pmp already probed. */
		TAILQ_FOREACH(pmp, &hammer2_pfslist, mntentry) {
			for (i = 0; i < HAMMER2_MAXCLUSTER; ++i) {
				if (pmp->pfs_names[i] &&
				    strcmp(pmp->pfs_names[i], label) == 0) {
					hmp = pmp->pfs_hmps[i];
					break;
				}
			}
			if (hmp)
				break;
		}
		if (hmp == NULL) {
			hprintf("PFS label \"%s\" not found\n", label);
			hammer2_cleanup_devvp(&devvpl);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			return (ENOENT);
		}
	}

	/*
	 * Open the device if this isn't a secondary mount and construct the
	 * HAMMER2 device mount (hmp).
	 */
	if (hmp == NULL) {
		/* Now open the device(s). */
		KKASSERT(!TAILQ_EMPTY(&devvpl));
		error = hammer2_open_devvp(mp, &devvpl);
		if (error) {
			hammer2_close_devvp(&devvpl);
			hammer2_cleanup_devvp(&devvpl);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			return (error);
		}

		/* Construct volumes and link with device vnodes. */
		hmp = malloc(sizeof(*hmp), M_HAMMER2, M_WAITOK | M_ZERO);
		hmp->devvp = NULL;
		error = hammer2_init_volumes(&devvpl, hmp->volumes,
		    &hmp->voldata, &hmp->devvp);
		if (error) {
			hammer2_close_devvp(&devvpl);
			hammer2_cleanup_devvp(&devvpl);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			free(hmp, M_HAMMER2);
			return (error);
		}
		if (!hmp->devvp) {
			hprintf("failed to initialize root volume\n");
			hammer2_unmount_helper(mp, NULL, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_unmount(mp, MNT_FORCE);
			return (EINVAL);
		}

		hmp->hflags = hflags & HMNT2_DEVFLAGS;
		KKASSERT(hmp->hflags & HMNT2_LOCAL);

		TAILQ_INSERT_TAIL(&hammer2_mntlist, hmp, mntentry);
		RB_INIT(&hmp->iotree);
		hammer2_mtx_init(&hmp->iotree_lock, "h2hmp_iotlk");

		/*
		 * vchain setup.  vchain.data is embedded.
		 * vchain.refs is initialized and will never drop to 0.
		 */
		hmp->vchain.hmp = hmp;
		hmp->vchain.refs = 1;
		hmp->vchain.data = (void *)&hmp->voldata;
		hmp->vchain.bref.type = HAMMER2_BREF_TYPE_VOLUME;
		hmp->vchain.bref.data_off = 0 | HAMMER2_PBUFRADIX;
		hmp->vchain.bref.mirror_tid = hmp->voldata.mirror_tid;
		hammer2_chain_init(&hmp->vchain);

		/* Initialize volume header related fields. */
		KKASSERT(hmp->voldata.magic == HAMMER2_VOLUME_ID_HBO ||
		    hmp->voldata.magic == HAMMER2_VOLUME_ID_ABO);
		/*
		 * Must use hmp instead of volume header for these two
		 * in order to handle volume versions transparently.
		 */
		if (hmp->voldata.version >= HAMMER2_VOL_VERSION_MULTI_VOLUMES) {
			hmp->nvolumes = hmp->voldata.nvolumes;
			hmp->total_size = hmp->voldata.total_size;
		} else {
			hmp->nvolumes = 1;
			hmp->total_size = hmp->voldata.volu_size;
		}
		KKASSERT(hmp->nvolumes > 0);

		/* Move devvpl entries to hmp. */
		TAILQ_INIT(&hmp->devvp_list);
		while ((e = TAILQ_FIRST(&devvpl)) != NULL) {
			TAILQ_REMOVE(&devvpl, e, entry);
			TAILQ_INSERT_TAIL(&hmp->devvp_list, e, entry);
		}
		KKASSERT(TAILQ_EMPTY(&devvpl));
		KKASSERT(!TAILQ_EMPTY(&hmp->devvp_list));

		/*
		 * Really important to get these right or teardown code
		 * will get confused.
		 */
		hmp->spmp = hammer2_pfsalloc(NULL, NULL, hmp);
		spmp = hmp->spmp;
		spmp->pfs_hmps[0] = hmp;

		/*
		 * Dummy-up vchain's modify_tid.
		 * mirror_tid is inherited from the volume header.
		 */
		hmp->vchain.bref.mirror_tid = hmp->voldata.mirror_tid;
		hmp->vchain.bref.modify_tid = hmp->vchain.bref.mirror_tid;
		hmp->vchain.pmp = spmp;

		/*
		 * First locate the super-root inode, which is key 0
		 * relative to the volume header's blockset.
		 *
		 * Then locate the root inode by scanning the directory keyspace
		 * represented by the label.
		 */
		parent = hammer2_chain_lookup_init(&hmp->vchain, 0);
		schain = hammer2_chain_lookup(&parent, &key_dummy,
		    HAMMER2_SROOT_KEY, HAMMER2_SROOT_KEY, &error, 0);
		hammer2_chain_lookup_done(parent);
		if (schain == NULL) {
			hprintf("invalid super-root\n");
			hammer2_unmount_helper(mp, NULL, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_unmount(mp, MNT_FORCE);
			return (EINVAL);
		}
		if (schain->error) {
			hprintf("chain error %08x reading super-root\n",
			    schain->error);
			hammer2_chain_unlock(schain);
			hammer2_chain_drop(schain);
			schain = NULL;
			hammer2_unmount_helper(mp, NULL, hmp);
			lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
			hammer2_unmount(mp, MNT_FORCE);
			return (EINVAL);
		}

		/*
		 * Sanity-check schain's pmp and finish initialization.
		 * Any chain belonging to the super-root topology should
		 * have a NULL pmp (not even set to spmp).
		 */
		ripdata = &schain->data->ipdata;
		KKASSERT(schain->pmp == NULL);
		spmp->pfs_clid = ripdata->meta.pfs_clid;

		/*
		 * Replace the dummy spmp->iroot with a real one.  It's
		 * easier to just do a wholesale replacement than to try
		 * to update the chain and fixup the iroot fields.
		 *
		 * The returned inode is locked with the supplied cluster.
		 */
		xop = uma_zalloc(zone_xops, M_WAITOK | M_ZERO);
		hammer2_dummy_xop_from_chain(xop, schain);
		hammer2_inode_drop(spmp->iroot);
		spmp->iroot = hammer2_inode_get(spmp, xop, -1, -1);
		spmp->spmp_hmp = hmp;
		spmp->pfs_types[0] = ripdata->meta.pfs_type;
		spmp->pfs_hmps[0] = hmp;
		hammer2_inode_ref(spmp->iroot);
		hammer2_inode_unlock(spmp->iroot);
		hammer2_chain_unlock(schain);
		hammer2_chain_drop(schain);
		schain = NULL;
		uma_zfree(zone_xops, xop);
		/* Leave spmp->iroot with one ref. */

		/*
		 * A false-positive lock order reversal may be detected.
		 * There are 2 directions of locking, which is a bad design.
		 * chain is locked -> hammer2_inode_get() -> lock inode
		 * inode is locked -> hammer2_inode_chain() -> lock chain
		 */
		hammer2_update_pmps(hmp);
	} else {
		spmp = hmp->spmp;
		if (hflags & HMNT2_DEVFLAGS)
			hprintf("Warning: mount flags pertaining to the whole "
			    "device may only be specified on the first mount "
			    "of the device: %08x\n",
			    hflags & HMNT2_DEVFLAGS);
	}

	/*
	 * Force local mount (disassociate all PFSs from their clusters)
	 * if HMNT2_LOCAL.
	 */
	force_local = (hmp->hflags & HMNT2_LOCAL) ? hmp : NULL;

	/*
	 * Lookup the mount point under the media-localized super-root.
	 * Scanning hammer2_pfslist doesn't help us because it represents
	 * PFS cluster ids which can aggregate several named PFSs together.
	 */
	hammer2_inode_lock(spmp->iroot, 0);
	parent = hammer2_inode_chain(spmp->iroot, 0, HAMMER2_RESOLVE_ALWAYS);
	lhc = hammer2_dirhash(label, strlen(label));
	chain = hammer2_chain_lookup(&parent, &key_next, lhc,
	    lhc + HAMMER2_DIRHASH_LOMASK, &error, 0);
	while (chain) {
		if (chain->bref.type == HAMMER2_BREF_TYPE_INODE &&
		    strcmp(label, chain->data->ipdata.filename) == 0)
			break;
		chain = hammer2_chain_next(&parent, chain, &key_next, key_next,
		    lhc + HAMMER2_DIRHASH_LOMASK, &error, 0);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
	hammer2_inode_unlock(spmp->iroot);

	/* PFS could not be found? */
	if (chain == NULL) {
		hammer2_unmount_helper(mp, NULL, hmp);
		lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
		hammer2_unmount(mp, MNT_FORCE);

		if (error) {
			hprintf("PFS label \"%s\" error %08x\n", label, error);
			return (EINVAL);
		} else {
			hprintf("PFS label \"%s\" not found\n", label);
			return (ENOENT);
		}
	}

	/* Acquire the pmp structure. */
	if (chain->error) {
		hprintf("PFS label \"%s\" chain error %08x\n",
		    label, chain->error);
	} else {
		ripdata = &chain->data->ipdata;
		pmp = hammer2_pfsalloc(NULL, ripdata, force_local);
	}
	hammer2_chain_unlock(chain);
	hammer2_chain_drop(chain);

	/* PFS to mount must exist at this point. */
	if (pmp == NULL) {
		hprintf("failed to acquire PFS structure\n");
		hammer2_unmount_helper(mp, NULL, hmp);
		lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
		hammer2_unmount(mp, MNT_FORCE);
		return (EINVAL);
	}

	/* Finish the mount. */
	debug_hprintf("hmp=%p pmp=%p\n", hmp, pmp);

	if (pmp->mp) {
		hprintf("PFS already mounted!\n");
		hammer2_unmount_helper(mp, NULL, hmp);
		lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);
		hammer2_unmount(mp, MNT_FORCE);
		return (EBUSY);
	}

	/*
	 * dev2udev(dev) alone isn't unique to PFS, but pfs_clid
	 * isn't either against multiple mounts with the same image.
	 */
	KKASSERT(!TAILQ_EMPTY(&hmp->devvp_list));
	dev = TAILQ_FIRST(&hmp->devvp_list)->devvp->v_rdev;
	KKASSERT(dev);
	mp->mnt_stat.f_fsid.val[0] = ((int32_t)dev2udev(dev)) ^
	    ((int32_t)pmp->pfs_clid.time_low);
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;

	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_kern_flag |= MNTK_LOOKUP_SHARED | MNTK_EXTENDED_SHARED |
	    MNTK_USES_BCACHE;
	MNT_IUNLOCK(mp);

	/* Required mount structure initializations. */
	mp->mnt_stat.f_iosize = HAMMER2_PBUFSIZE;
	mp->mnt_stat.f_bsize = HAMMER2_PBUFSIZE;

	/* Connect up mount pointers. */
	hammer2_mount_helper(mp, pmp);
	lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);

	/* Initial statfs to prime mnt_stat. */
	hammer2_statfs(mp, &mp->mnt_stat);

	strlcpy(pmp->mntpt, mntpt, sizeof(pmp->mntpt));
	vfs_mountedfrom(mp, fspec);

	return (0);
}

/*
 * Scan PFSs under the super-root and create hammer2_pfs structures.
 */
static void
hammer2_update_pmps(hammer2_dev_t *hmp)
{
	hammer2_dev_t *force_local;
	hammer2_pfs_t *spmp;
	const hammer2_inode_data_t *ripdata;
	hammer2_chain_t *parent;
	hammer2_chain_t *chain;
	hammer2_key_t key_next;
	int error;

	/*
	 * Force local mount (disassociate all PFSs from their clusters)
	 * if HMNT2_LOCAL.
	 */
	force_local = (hmp->hflags & HMNT2_LOCAL) ? hmp : NULL;

	/* Lookup mount point under the media-localized super-root. */
	spmp = hmp->spmp;
	hammer2_inode_lock(spmp->iroot, 0);
	parent = hammer2_inode_chain(spmp->iroot, 0, HAMMER2_RESOLVE_ALWAYS);
	chain = hammer2_chain_lookup(&parent, &key_next, HAMMER2_KEY_MIN,
	    HAMMER2_KEY_MAX, &error, 0);
	while (chain) {
		if (chain->error) {
			hprintf("chain error %08x reading PFS root\n",
			    chain->error);
		} else if (chain->bref.type != HAMMER2_BREF_TYPE_INODE) {
			hprintf("non inode chain type %d under super-root\n",
			    chain->bref.type);
		} else {
			ripdata = &chain->data->ipdata;
			hammer2_pfsalloc(chain, ripdata, force_local);
		}
		chain = hammer2_chain_next(&parent, chain, &key_next, key_next,
		    HAMMER2_KEY_MAX, &error, 0);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
	hammer2_inode_unlock(spmp->iroot);
}

static int
hammer2_unmount(struct mount *mp, int mntflags)
{
	hammer2_pfs_t *pmp = MPTOPMP(mp);
	int flags = 0, error = 0;

	/* Still NULL during mount before hammer2_mount_helper() called. */
	if (pmp == NULL)
		return(0);

	KKASSERT(pmp->mp);
	KKASSERT(pmp->iroot);

	lockmgr(&hammer2_mntlk, LK_EXCLUSIVE, NULL);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	error = vflush(mp, 0, flags, curthread);
	if (error) {
		hprintf("vflush failed %d\n", error);
		goto failed;
	}

	hammer2_unmount_helper(mp, pmp, NULL);
failed:
	lockmgr(&hammer2_mntlk, LK_RELEASE, NULL);

	if (TAILQ_EMPTY(&hammer2_mntlist))
		hammer2_assert_clean();

	return (error);
}

/*
 * Mount helper, hook the system mount into our PFS.
 * The mount lock is held.
 *
 * We must bump the mount_count on related devices for any mounted PFSs.
 */
static void
hammer2_mount_helper(struct mount *mp, hammer2_pfs_t *pmp)
{
	hammer2_cluster_t *cluster;
	hammer2_chain_t *rchain;
	int i;

	mp->mnt_data = (qaddr_t)pmp;
	pmp->mp = mp;

	/* After pmp->mp is set adjust hmp->mount_count. */
	cluster = &pmp->iroot->cluster;
	for (i = 0; i < cluster->nchains; ++i) {
		rchain = cluster->array[i].chain;
		if (rchain == NULL)
			continue;
		++rchain->hmp->mount_count;
	}
}

/*
 * Unmount helper, unhook the system mount from our PFS.
 * The mount lock is held.
 *
 * If hmp is supplied a mount responsible for being the first to open
 * the block device failed and the block device and all PFSs using the
 * block device must be cleaned up.
 *
 * If pmp is supplied multiple devices might be backing the PFS and each
 * must be disconnected.  This might not be the last PFS using some of the
 * underlying devices.  Also, we have to adjust our hmp->mount_count
 * accounting for the devices backing the pmp which is now undergoing an
 * unmount.
 */
static void
hammer2_unmount_helper(struct mount *mp, hammer2_pfs_t *pmp, hammer2_dev_t *hmp)
{
	hammer2_cluster_t *cluster;
	hammer2_chain_t *rchain;
	int i, dumpcnt __diagused;

	/*
	 * If no device supplied this is a high-level unmount and we have to
	 * to disconnect the mount, adjust mount_count, and locate devices
	 * that might now have no mounts.
	 */
	if (pmp) {
		KKASSERT(hmp == NULL);
		KKASSERT(MPTOPMP(mp) == pmp);
		pmp->mp = NULL;
		mp->mnt_data = NULL;

		/*
		 * After pmp->mp is cleared we have to account for
		 * mount_count.
		 */
		cluster = &pmp->iroot->cluster;
		for (i = 0; i < cluster->nchains; ++i) {
			rchain = cluster->array[i].chain;
			if (rchain == NULL)
				continue;
			--rchain->hmp->mount_count;
			/* Scrapping hmp now may invalidate the pmp. */
		}
again:
		TAILQ_FOREACH(hmp, &hammer2_mntlist, mntentry) {
			if (hmp->mount_count == 0) {
				hammer2_unmount_helper(NULL, NULL, hmp);
				goto again;
			}
		}
		return;
	}

	/*
	 * Try to terminate the block device.  We can't terminate it if
	 * there are still PFSs referencing it.
	 */
	if (hmp->mount_count) {
		hprintf("%d PFS mounts still exist\n", hmp->mount_count);
		return;
	}

	hammer2_pfsfree_scan(hmp, 0);
	hammer2_pfsfree_scan(hmp, 1);
	KKASSERT(hmp->spmp == NULL);

	/* Finish up with the device vnode. */
	if (!TAILQ_EMPTY(&hmp->devvp_list)) {
		hammer2_close_devvp(&hmp->devvp_list);
		hammer2_cleanup_devvp(&hmp->devvp_list);
	}
	KKASSERT(TAILQ_EMPTY(&hmp->devvp_list));
#ifdef INVARIANTS
	/*
	 * Final drop of embedded volume root chain to clean up
	 * vchain.core (vchain structure is not flagged ALLOCATED
	 * so it is cleaned out and then left to rot).
	 */
	dumpcnt = 50;
	hammer2_dump_chain(&hmp->vchain, 0, 0, &dumpcnt, 'v', (unsigned int)-1);
	hammer2_chain_drop(&hmp->vchain);
#endif
	hammer2_mtx_ex(&hmp->iotree_lock);
	hammer2_io_cleanup(hmp, &hmp->iotree);
	if (hmp->iofree_count)
		debug_hprintf("%d I/O's left hanging\n", hmp->iofree_count);
	hammer2_mtx_unlock(&hmp->iotree_lock);

	TAILQ_REMOVE(&hammer2_mntlist, hmp, mntentry);
	hammer2_mtx_destroy(&hmp->iotree_lock);

	free(hmp, M_HAMMER2);
}

static int
hammer2_vget(struct mount *mp, ino_t ino, int flags, struct vnode **vpp)
{
	hammer2_pfs_t *pmp = MPTOPMP(mp);
	hammer2_inode_t *ip;
	hammer2_xop_lookup_t *xop;
	hammer2_tid_t inum;
	int error;

	inum = (hammer2_tid_t)ino & HAMMER2_DIRHASH_USERMSK;

	/* Easy if we already have it cached. */
	ip = hammer2_inode_lookup(pmp, inum);
	if (ip) {
		hammer2_inode_lock(ip, HAMMER2_RESOLVE_SHARED);
		error = hammer2_igetv(ip, flags, vpp);
		hammer2_inode_unlock(ip);
		hammer2_inode_drop(ip); /* from lookup */
		return (error);
	}

	/* Otherwise we have to find the inode. */
	xop = hammer2_xop_alloc(pmp->iroot);
	xop->lhc = inum;
	hammer2_xop_start(&xop->head, &hammer2_lookup_desc);
	error = hammer2_xop_collect(&xop->head, 0);

	if (error == 0)
		ip = hammer2_inode_get(pmp, &xop->head, -1, -1);
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);

	if (ip) {
		error = hammer2_igetv(ip, flags, vpp);
		hammer2_inode_unlock(ip);
	} else {
		*vpp = NULL;
		error = ENOENT;
	}

	return (error);
}

static int
hammer2_root(struct mount *mp, int flags, struct vnode **vpp)
{
	hammer2_pfs_t *pmp = MPTOPMP(mp);
	int error;

	if (pmp->iroot == NULL) {
		hprintf("%s has no root inode\n", mp->mnt_stat.f_mntfromname);
		*vpp = NULL;
		return (EINVAL);
	}

	hammer2_inode_lock(pmp->iroot, HAMMER2_RESOLVE_SHARED);
	error = hammer2_igetv(pmp->iroot, LK_EXCLUSIVE, vpp);
	hammer2_inode_unlock(pmp->iroot);

	return (error);
}

static int
hammer2_statfs(struct mount *mp, struct statfs *sbp)
{
	hammer2_pfs_t *pmp = MPTOPMP(mp);
	hammer2_dev_t *hmp;
	hammer2_cluster_t *cluster;
	hammer2_chain_t *chain;

	KKASSERT(mp->mnt_stat.f_iosize > 0);
	KKASSERT(mp->mnt_stat.f_bsize > 0);

	hmp = pmp->pfs_hmps[0];
	if (hmp == NULL)
		return (EINVAL);

	cluster = &pmp->iroot->cluster;
	hammer2_assert_cluster(cluster);

	chain = cluster->array[0].chain;

	sbp->f_bsize = mp->mnt_stat.f_bsize;
	sbp->f_iosize = mp->mnt_stat.f_iosize;
	sbp->f_blocks = hmp->voldata.allocator_size / mp->mnt_stat.f_bsize;
	sbp->f_bfree = hmp->voldata.allocator_free / mp->mnt_stat.f_bsize;
	sbp->f_bavail = sbp->f_bfree;
	sbp->f_files = chain ? chain->bref.embed.stats.inode_count : 0;
	sbp->f_ffree = 0;

	return (0);
}

static int
hammer2_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{
	hammer2_inode_t *ip;
	hammer2_tid_t inum;
	int error;

	inum = ((hammer2_tid_t *)fhp->fid_data)[0] & HAMMER2_DIRHASH_USERMSK;
	if (vpp) {
		if (inum == 1)
			error = hammer2_root(mp, LK_EXCLUSIVE, vpp);
		else
			error = hammer2_vget(mp, inum, LK_EXCLUSIVE, vpp);
	} else {
		error = 0;
	}

	ip = VTOI(*vpp);
	vnode_create_vobject(*vpp, ip->meta.size, curthread);

	return (error);
}

static struct vfsops hammer2_vfsops = {
	.vfs_init	= hammer2_init,
	.vfs_uninit	= hammer2_uninit,
	.vfs_mount	= hammer2_mount,
	.vfs_unmount	= hammer2_unmount,
	.vfs_vget	= hammer2_vget,
	.vfs_root	= hammer2_root,
	.vfs_statfs	= hammer2_statfs,
	.vfs_fhtovp	= hammer2_fhtovp,
};

VFS_SET(hammer2_vfsops, hammer2, VFCF_READONLY);
MODULE_VERSION(hammer2, 1);
