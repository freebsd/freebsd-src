/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
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

#include "hammer2.h"
#include "hammer2_ioctl.h"

/*
 * Retrieve ondisk version.
 */
static int
hammer2_ioctl_version_get(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_version_t *version = data;
	hammer2_dev_t *hmp = ip->pmp->pfs_hmps[0];

	if (hmp == NULL)
		return (EINVAL);

	if (hmp)
		version->version = hmp->voldata.version;
	else
		version->version = -1;

	return (0);
}

/*
 * Used to scan and retrieve PFS information.  PFS's are directories under
 * the super-root.
 *
 * To scan PFSs pass name_key=0.  The function will scan for the next
 * PFS and set all fields, as well as set name_next to the next key.
 * When no PFSs remain, name_next is set to (hammer2_key_t)-1.
 *
 * To retrieve a particular PFS by key, specify the key but note that
 * the ioctl will return the lowest key >= specified_key, so the caller
 * must verify the key.
 *
 * To retrieve the PFS associated with the file descriptor, pass
 * name_key set to (hammer2_key_t)-1.
 */
static int
hammer2_ioctl_pfs_get(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_pfs_t *pfs = data;
	hammer2_dev_t *hmp = ip->pmp->pfs_hmps[0];
	const hammer2_inode_data_t *ripdata;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t key_next, save_key;
	int error = 0;

	if (hmp == NULL)
		return (EINVAL);

	save_key = pfs->name_key;

	if (save_key == (hammer2_key_t)-1) {
		hammer2_inode_lock(ip->pmp->iroot, 0);
		parent = NULL;
		chain = hammer2_inode_chain(ip->pmp->iroot, 0,
		    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	} else {
		hammer2_inode_lock(hmp->spmp->iroot, 0);
		parent = hammer2_inode_chain(hmp->spmp->iroot, 0,
		    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
		chain = hammer2_chain_lookup(&parent, &key_next, pfs->name_key,
		    HAMMER2_KEY_MAX, &error, HAMMER2_LOOKUP_SHARED);
	}

	/* Locate next PFS. */
	while (chain) {
		if (chain->bref.type == HAMMER2_BREF_TYPE_INODE)
			break;
		if (parent == NULL) {
			hammer2_chain_unlock(chain);
			hammer2_chain_drop(chain);
			chain = NULL;
			break;
		}
		chain = hammer2_chain_next(&parent, chain, &key_next, key_next,
		    HAMMER2_KEY_MAX, &error, HAMMER2_LOOKUP_SHARED);
	}
	error = hammer2_error_to_errno(error);

	/* Load the data being returned by the ioctl. */
	if (chain && chain->error == 0) {
		ripdata = &chain->data->ipdata;
		pfs->name_key = ripdata->meta.name_key;
		pfs->pfs_type = ripdata->meta.pfs_type;
		pfs->pfs_subtype = ripdata->meta.pfs_subtype;
		pfs->pfs_clid = ripdata->meta.pfs_clid;
		pfs->pfs_fsid = ripdata->meta.pfs_fsid;
		KKASSERT(ripdata->meta.name_len < sizeof(pfs->name));
		bcopy(ripdata->filename, pfs->name, ripdata->meta.name_len);
		pfs->name[ripdata->meta.name_len] = 0;

		/*
		 * Calculate name_next, if any.  We are only accessing
		 * chain->bref so we can ignore chain->error (if the key
		 * is used later it will error then).
		 */
		if (parent == NULL) {
			pfs->name_next = (hammer2_key_t)-1;
		} else {
			chain = hammer2_chain_next(&parent, chain, &key_next,
			    key_next, HAMMER2_KEY_MAX, &error,
			    HAMMER2_LOOKUP_SHARED);
			if (chain)
				pfs->name_next = chain->bref.key;
			else
				pfs->name_next = (hammer2_key_t)-1;
		}
	} else {
		pfs->name_next = (hammer2_key_t)-1;
		error = ENOENT;
	}

	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}

	if (save_key == (hammer2_key_t)-1)
		hammer2_inode_unlock(ip->pmp->iroot);
	else
		hammer2_inode_unlock(hmp->spmp->iroot);

	return (error);
}

/*
 * Find a specific PFS by name.
 */
static int
hammer2_ioctl_pfs_lookup(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_pfs_t *pfs = data;
	hammer2_dev_t *hmp = ip->pmp->pfs_hmps[0];
	const hammer2_inode_data_t *ripdata;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t key_next, lhc;
	size_t len;
	int error = 0;

	if (hmp == NULL)
		return (EINVAL);

	hammer2_inode_lock(hmp->spmp->iroot, HAMMER2_RESOLVE_SHARED);
	parent = hammer2_inode_chain(hmp->spmp->iroot, 0,
	    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);

	pfs->name[sizeof(pfs->name) - 1] = 0;
	len = strlen(pfs->name);
	lhc = hammer2_dirhash(pfs->name, len);

	chain = hammer2_chain_lookup(&parent, &key_next, lhc,
	    lhc + HAMMER2_DIRHASH_LOMASK, &error, HAMMER2_LOOKUP_SHARED);
	while (chain) {
		if (hammer2_chain_dirent_test(chain, pfs->name, len))
			break;
		chain = hammer2_chain_next(&parent, chain, &key_next, key_next,
		    lhc + HAMMER2_DIRHASH_LOMASK, &error,
		    HAMMER2_LOOKUP_SHARED);
	}
	error = hammer2_error_to_errno(error);

	/* Load the data being returned by the ioctl. */
	if (chain && chain->error == 0) {
		KKASSERT(chain->bref.type == HAMMER2_BREF_TYPE_INODE);
		ripdata = &chain->data->ipdata;
		pfs->name_key = ripdata->meta.name_key;
		pfs->pfs_type = ripdata->meta.pfs_type;
		pfs->pfs_subtype = ripdata->meta.pfs_subtype;
		pfs->pfs_clid = ripdata->meta.pfs_clid;
		pfs->pfs_fsid = ripdata->meta.pfs_fsid;

		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	} else if (error == 0) {
		error = ENOENT;
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}

	hammer2_inode_unlock(hmp->spmp->iroot);

	return (error);
}

/*
 * Retrieve the raw inode structure, non-inclusive of node-specific data.
 */
static int
hammer2_ioctl_inode_get(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_inode_t *ino = data;

	hammer2_inode_lock(ip, HAMMER2_RESOLVE_SHARED);

	ino->data_count = hammer2_inode_data_count(ip);
	ino->inode_count = hammer2_inode_inode_count(ip);
	bzero(&ino->ip_data, sizeof(ino->ip_data));
	ino->ip_data.meta = ip->meta;

	hammer2_inode_unlock(ip);

	return (0);
}

/*
 * Recursively dump chains of a given inode.
 */
static int
hammer2_ioctl_debug_dump(hammer2_inode_t *ip, unsigned int flags)
{
#ifdef INVARIANTS
	hammer2_chain_t *chain;
	int i, count = 100000;

	for (i = 0; i < ip->cluster.nchains; ++i) {
		chain = ip->cluster.array[i].chain;
		if (chain) {
			hprintf("cluster #%d\n", i);
			hammer2_dump_chain(chain, 0, 0, &count, 'i', flags);
		}
	}

	return (0);
#else
	return (EOPNOTSUPP);
#endif
}

/*
 * Get a list of volumes.
 */
static int
hammer2_ioctl_volume_list(hammer2_inode_t *ip, void *data)
{
	hammer2_ioc_volume_list_t *vollist = data;
	hammer2_ioc_volume_t entry;
	hammer2_volume_t *vol;
	hammer2_dev_t *hmp = ip->pmp->pfs_hmps[0];
	int i, error = 0, cnt = 0;

	if (hmp == NULL)
		return (EINVAL);

	for (i = 0; i < hmp->nvolumes; ++i) {
		if (cnt >= vollist->nvolumes)
			break;
		vol = &hmp->volumes[i];
		bzero(&entry, sizeof(entry));
		/* Copy hammer2_volume_t fields. */
		entry.id = vol->id;
		bcopy(vol->dev->path, entry.path, sizeof(entry.path));
		entry.offset = vol->offset;
		entry.size = vol->size;
		error = copyout(&entry, &vollist->volumes[cnt], sizeof(entry));
		if (error)
			return (error);
		cnt++;
	}
	vollist->nvolumes = cnt;
	vollist->version = hmp->voldata.version;
	bcopy(ip->pmp->pfs_names[0], vollist->pfs_name,
	    sizeof(vollist->pfs_name));

	return (error);
}

int
hammer2_ioctl_impl(hammer2_inode_t *ip, unsigned long com, void *data,
    int fflag, struct ucred *cred)
{
	int error;

	switch (com) {
	case HAMMER2IOC_VERSION_GET:
		error = hammer2_ioctl_version_get(ip, data);
		break;
	case HAMMER2IOC_PFS_GET:
		error = hammer2_ioctl_pfs_get(ip, data);
		break;
	case HAMMER2IOC_PFS_LOOKUP:
		error = hammer2_ioctl_pfs_lookup(ip, data);
		break;
	case HAMMER2IOC_INODE_GET:
		error = hammer2_ioctl_inode_get(ip, data);
		break;
	case HAMMER2IOC_DEBUG_DUMP:
		error = hammer2_ioctl_debug_dump(ip, *(unsigned int *)data);
		break;
	case HAMMER2IOC_VOLUME_LIST:
		error = hammer2_ioctl_volume_list(ip, data);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}
