/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Juniper Networks, Inc.
 * Copyright (c) 2022-2023 Klara, Inc.
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
 */

#include "opt_tarfs.h"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm_param.h>

#include <fs/tarfs/tarfs.h>
#include <fs/tarfs/tarfs_dbg.h>

MALLOC_DEFINE(M_TARFSNAME, "tarfs name", "tarfs file names");
MALLOC_DEFINE(M_TARFSBLK, "tarfs blk", "tarfs block maps");

SYSCTL_NODE(_vfs, OID_AUTO, tarfs, CTLFLAG_RW, 0, "Tar filesystem");

unsigned int tarfs_ioshift = TARFS_IOSHIFT_DEFAULT;

static int
tarfs_sysctl_handle_ioshift(SYSCTL_HANDLER_ARGS)
{
	unsigned int tmp;
	int error;

	tmp = *(unsigned int *)arg1;
	if ((error = SYSCTL_OUT(req, &tmp, sizeof(tmp))) != 0)
		return (error);
	if (req->newptr != NULL) {
		if ((error = SYSCTL_IN(req, &tmp, sizeof(tmp))) != 0)
			return (error);
		if (tmp == 0)
			tmp = TARFS_IOSHIFT_DEFAULT;
		if (tmp < TARFS_IOSHIFT_MIN)
			tmp = TARFS_IOSHIFT_MIN;
		if (tmp > TARFS_IOSHIFT_MAX)
			tmp = TARFS_IOSHIFT_MAX;
		*(unsigned int *)arg1 = tmp;
	}
	return (0);
}

SYSCTL_PROC(_vfs_tarfs, OID_AUTO, ioshift,
    CTLTYPE_UINT | CTLFLAG_MPSAFE | CTLFLAG_RWTUN,
    &tarfs_ioshift, 0, tarfs_sysctl_handle_ioshift, "IU",
    "Tar filesystem preferred I/O size (log 2)");

#ifdef TARFS_DEBUG
int tarfs_debug;
SYSCTL_INT(_vfs_tarfs, OID_AUTO, debug, CTLFLAG_RWTUN,
    &tarfs_debug, 0, "Tar filesystem debug mask");
#endif	/* TARFS_DEBUG */

struct tarfs_node *
tarfs_lookup_node(struct tarfs_node *tnp, struct tarfs_node *f,
    struct componentname *cnp)
{
	boolean_t found;
	struct tarfs_node *entry;

	TARFS_DPF(LOOKUP, "%s: name: %.*s\n", __func__, (int)cnp->cn_namelen,
	    cnp->cn_nameptr);

	found = false;
	TAILQ_FOREACH(entry, &tnp->dir.dirhead, dirents) {
		if (f != NULL && entry != f)
			continue;

		if (entry->namelen == cnp->cn_namelen &&
		    bcmp(entry->name, cnp->cn_nameptr,
		    entry->namelen) == 0) {
			found = 1;
			break;
		}
	}

	if (found) {
		if (entry->type == VREG && entry->other != NULL) {
			TARFS_DPF_IFF(LOOKUP, "%s: following hard link %p\n",
			    __func__, entry);
			entry = entry->other;
		}
		TARFS_DPF(LOOKUP, "%s: found tarfs_node %p\n", __func__,
		    entry);
		return (entry);
	}

	TARFS_DPF(LOOKUP, "%s: no match found\n", __func__);
	return (NULL);
}

struct tarfs_node *
tarfs_lookup_dir(struct tarfs_node *tnp, off_t cookie)
{
	struct tarfs_node *current;

	TARFS_DPF(LOOKUP, "%s: tarfs_node %p, cookie %jd\n", __func__, tnp,
	    cookie);
	TARFS_DPF(LOOKUP, "%s: name: %s\n", __func__,
	    (tnp->name == NULL) ? "<<root>>" : tnp->name);

	if (cookie == tnp->dir.lastcookie &&
	    tnp->dir.lastnode != NULL) {
		TARFS_DPF(LOOKUP, "%s: Using cached entry: tarfs_node %p, "
		    "cookie %jd\n", __func__, tnp->dir.lastnode,
		    tnp->dir.lastcookie);
		return (tnp->dir.lastnode);
	}

	TAILQ_FOREACH(current, &tnp->dir.dirhead, dirents) {
		TARFS_DPF(LOOKUP, "%s: tarfs_node %p, current %p, ino %lu\n",
		    __func__, tnp, current, current->ino);
		TARFS_DPF_IFF(LOOKUP, current->name != NULL,
		    "%s: name: %s\n", __func__, current->name);
		if (current->ino == cookie) {
			TARFS_DPF(LOOKUP, "%s: Found entry: tarfs_node %p, "
			    "cookie %lu\n", __func__, current,
			    current->ino);
			break;
		}
	}

	return (current);
}

int
tarfs_alloc_node(struct tarfs_mount *tmp, const char *name, size_t namelen,
    __enum_uint8(vtype) type, off_t off, size_t sz, time_t mtime, uid_t uid, gid_t gid,
    mode_t mode, unsigned int flags, const char *linkname, dev_t rdev,
    struct tarfs_node *parent, struct tarfs_node **retnode)
{
	struct tarfs_node *tnp;

	TARFS_DPF(ALLOC, "%s(%.*s)\n", __func__, (int)namelen, name);

	if (parent != NULL && parent->type != VDIR)
		return (ENOTDIR);
	tnp = malloc(sizeof(struct tarfs_node), M_TARFSNODE, M_WAITOK | M_ZERO);
	mtx_init(&tnp->lock, "tarfs node lock", NULL, MTX_DEF);
	tnp->gen = arc4random();
	tnp->tmp = tmp;
	if (namelen > 0) {
		tnp->name = malloc(namelen + 1, M_TARFSNAME, M_WAITOK);
		tnp->namelen = namelen;
		memcpy(tnp->name, name, namelen);
		tnp->name[namelen] = '\0';
	}
	tnp->type = type;
	tnp->uid = uid;
	tnp->gid = gid;
	tnp->mode = mode;
	tnp->nlink = 1;
	vfs_timestamp(&tnp->atime);
	tnp->mtime.tv_sec = mtime;
	tnp->birthtime = tnp->atime;
	tnp->ctime = tnp->mtime;
	if (parent != NULL) {
		tnp->ino = alloc_unr(tmp->ino_unr);
	}
	tnp->offset = off;
	tnp->size = tnp->physize = sz;
	switch (type) {
	case VDIR:
		MPASS(parent != tnp);
		MPASS(parent != NULL || tmp->root == NULL);
		TAILQ_INIT(&tnp->dir.dirhead);
		tnp->nlink++;
		if (parent == NULL) {
			tnp->ino = TARFS_ROOTINO;
		}
		tnp->physize = 0;
		break;
	case VLNK:
		tnp->link.name = malloc(sz + 1, M_TARFSNAME,
		    M_WAITOK);
		tnp->link.namelen = sz;
		memcpy(tnp->link.name, linkname, sz);
		tnp->link.name[sz] = '\0';
		break;
	case VREG:
		/* create dummy block map */
		tnp->nblk = 1;
		tnp->blk = malloc(sizeof(*tnp->blk), M_TARFSBLK, M_WAITOK);
		tnp->blk[0].i = 0;
		tnp->blk[0].o = 0;
		tnp->blk[0].l = tnp->physize;
		break;
	case VFIFO:
		/* Nothing extra to do */
		break;
	case VBLK:
	case VCHR:
		tnp->rdev = rdev;
		tnp->physize = 0;
		break;
	default:
		panic("%s: type %d not allowed", __func__, type);
	}
	if (parent != NULL) {
		TARFS_NODE_LOCK(parent);
		TAILQ_INSERT_TAIL(&parent->dir.dirhead, tnp, dirents);
		parent->size += sizeof(struct tarfs_node);
		tnp->parent = parent;
		if (type == VDIR) {
			parent->nlink++;
		}
		TARFS_NODE_UNLOCK(parent);
	} else {
		tnp->parent = tnp;
	}
	MPASS(tnp->ino != 0);

	TARFS_ALLNODES_LOCK(tmp);
	TAILQ_INSERT_TAIL(&tmp->allnodes, tnp, entries);
	TARFS_ALLNODES_UNLOCK(tmp);

	*retnode = tnp;
	tmp->nfiles++;
	return (0);
}

#define is09(ch) ((ch) >= '0' && (ch) <= '9')

int
tarfs_load_blockmap(struct tarfs_node *tnp, size_t realsize)
{
	struct tarfs_blk *blk = NULL;
	char *map = NULL;
	size_t nmap = 0, nblk = 0;
	char *p, *q;
	ssize_t res;
	unsigned int i;
	long n;

	/*
	 * Load the entire map into memory.  We don't know how big it is,
	 * but as soon as we start reading it we will know how many
	 * entries it contains, and then we can count newlines.
	 */
	do {
		nmap++;
		if (tnp->size < nmap * TARFS_BLOCKSIZE) {
			TARFS_DPF(MAP, "%s: map too large\n", __func__);
			goto bad;
		}
		/* grow the map */
		map = realloc(map, nmap * TARFS_BLOCKSIZE + 1, M_TARFSBLK,
		    M_ZERO | M_WAITOK);
		/* read an additional block */
		res = tarfs_io_read_buf(tnp->tmp, false,
		    map + (nmap - 1) * TARFS_BLOCKSIZE,
		    tnp->offset + (nmap - 1) * TARFS_BLOCKSIZE,
		    TARFS_BLOCKSIZE);
		if (res < 0)
			return (-res);
		else if (res < TARFS_BLOCKSIZE)
			return (EIO);
		map[nmap * TARFS_BLOCKSIZE] = '\0'; /* sentinel */
		if (nblk == 0) {
			n = strtol(p = map, &q, 10);
			if (q == p || *q != '\n' || n < 1)
				goto syntax;
			nblk = n;
		}
		for (n = 0, p = map; *p != '\0'; ++p) {
			if (*p == '\n') {
				++n;
			}
		}
		TARFS_DPF(MAP, "%s: %ld newlines in map\n", __func__, n);
	} while (n < nblk * 2 + 1);
	TARFS_DPF(MAP, "%s: block map length %zu\n", __func__, nblk);
	blk = malloc(sizeof(*blk) * nblk, M_TARFSBLK, M_WAITOK | M_ZERO);
	p = strchr(map, '\n') + 1;
	for (i = 0; i < nblk; i++) {
		if (i == 0)
			blk[i].i = nmap * TARFS_BLOCKSIZE;
		else
			blk[i].i = blk[i - 1].i + blk[i - 1].l;
		n = strtol(p, &q, 10);
		if (q == p || *q != '\n' || n < 0)
			goto syntax;
		p = q + 1;
		blk[i].o = n;
		n = strtol(p, &q, 10);
		if (q == p || *q != '\n' || n < 0)
			goto syntax;
		p = q + 1;
		blk[i].l = n;
		TARFS_DPF(MAP, "%s: %3d %12zu %12zu %12zu\n", __func__,
		    i, blk[i].i, blk[i].o, blk[i].l);
		/*
		 * Check block alignment if the block is of non-zero
		 * length (a zero-length block indicates the end of a
		 * trailing hole).  Checking i indirectly checks the
		 * previous block's l.  It's ok for the final block to
		 * have an uneven length.
		 */
		if (blk[i].l == 0) {
			TARFS_DPF(MAP, "%s: zero-length block\n", __func__);
		} else if (blk[i].i % TARFS_BLOCKSIZE != 0 ||
		    blk[i].o % TARFS_BLOCKSIZE != 0) {
			TARFS_DPF(MAP, "%s: misaligned map entry\n", __func__);
			goto bad;
		}
		/*
		 * Check that this block starts after the end of the
		 * previous one.
		 */
		if (i > 0 && blk[i].o < blk[i - 1].o + blk[i - 1].l) {
			TARFS_DPF(MAP, "%s: overlapping map entries\n", __func__);
			goto bad;
		}
		/*
		 * Check that the block is within the file, both
		 * physically and logically.
		 */
		if (blk[i].i + blk[i].l > tnp->physize ||
		    blk[i].o + blk[i].l > realsize) {
			TARFS_DPF(MAP, "%s: map overflow\n", __func__);
			goto bad;
		}
	}
	free(map, M_TARFSBLK);

	/* store in node */
	free(tnp->blk, M_TARFSBLK);
	tnp->nblk = nblk;
	tnp->blk = blk;
	tnp->size = realsize;
	return (0);
syntax:
	TARFS_DPF(MAP, "%s: syntax error in block map\n", __func__);
bad:
	free(map, M_TARFSBLK);
	free(blk, M_TARFSBLK);
	return (EINVAL);
}

void
tarfs_free_node(struct tarfs_node *tnp)
{
	struct tarfs_mount *tmp;

	MPASS(tnp != NULL);
	tmp = tnp->tmp;

	switch (tnp->type) {
	case VREG:
		if (tnp->nlink-- > 1)
			return;
		if (tnp->other != NULL)
			tarfs_free_node(tnp->other);
		break;
	case VDIR:
		if (tnp->nlink-- > 2)
			return;
		if (tnp->parent != NULL && tnp->parent != tnp)
			tarfs_free_node(tnp->parent);
		break;
	case VLNK:
		if (tnp->link.name)
			free(tnp->link.name, M_TARFSNAME);
		break;
	default:
		break;
	}
	if (tnp->name != NULL)
		free(tnp->name, M_TARFSNAME);
	if (tnp->blk != NULL)
		free(tnp->blk, M_TARFSBLK);
	if (tnp->ino >= TARFS_MININO)
		free_unr(tmp->ino_unr, tnp->ino);
	TAILQ_REMOVE(&tmp->allnodes, tnp, entries);
	free(tnp, M_TARFSNODE);
	tmp->nfiles--;
}

int
tarfs_read_file(struct tarfs_node *tnp, size_t len, struct uio *uiop)
{
	struct uio auio;
	size_t resid = len;
	size_t copylen;
	unsigned int i;
	int error;

	TARFS_DPF(VNODE, "%s(%s, %zu, %zu)\n", __func__,
	    tnp->name, uiop->uio_offset, resid);
	for (i = 0; i < tnp->nblk && resid > 0; ++i) {
		if (uiop->uio_offset > tnp->blk[i].o + tnp->blk[i].l) {
			/* skip this block */
			continue;
		}
		while (resid > 0 &&
		    uiop->uio_offset < tnp->blk[i].o) {
			/* move out some zeroes... */
			copylen = tnp->blk[i].o - uiop->uio_offset;
			if (copylen > resid)
				copylen = resid;
			if (copylen > ZERO_REGION_SIZE)
				copylen = ZERO_REGION_SIZE;
			auio = *uiop;
			auio.uio_offset = 0;
			auio.uio_resid = copylen;
			error = uiomove(__DECONST(void *, zero_region),
			    copylen, &auio);
			if (error != 0)
				return (error);
			TARFS_DPF(MAP, "%s(%s) = zero %zu\n", __func__,
			    tnp->name, copylen - auio.uio_resid);
			uiop->uio_offset += copylen - auio.uio_resid;
			uiop->uio_resid -= copylen - auio.uio_resid;
			resid -= copylen - auio.uio_resid;
		}
		while (resid > 0 &&
		    uiop->uio_offset < tnp->blk[i].o + tnp->blk[i].l) {
			/* now actual data */
			copylen = tnp->blk[i].l;
			if (copylen > resid)
				copylen = resid;
			auio = *uiop;
			auio.uio_offset = tnp->offset + tnp->blk[i].i +
			    uiop->uio_offset - tnp->blk[i].o;
			auio.uio_resid = copylen;
			error = tarfs_io_read(tnp->tmp, false, &auio);
			if (error != 0)
				return (error);
			TARFS_DPF(MAP, "%s(%s) = data %zu\n", __func__,
			    tnp->name, copylen - auio.uio_resid);
			uiop->uio_offset += copylen - auio.uio_resid;
			uiop->uio_resid -= copylen - auio.uio_resid;
			resid -= copylen - auio.uio_resid;
		}
	}
	TARFS_DPF(VNODE, "%s(%s) = %zu\n", __func__,
	    tnp->name, len - resid);
	return (0);
}

/*
 * XXX ugly file flag parser which could easily be a finite state machine
 * driven by a small precomputed table.
 *
 * Note that unlike strtofflags(3), we make no attempt to handle negated
 * flags, since they shouldn't appear in tar files.
 */
static const struct tarfs_flag {
	const char *name;
	unsigned int flag;
} tarfs_flags[] = {
	{ "nodump",	UF_NODUMP },
	{ "uchg",	UF_IMMUTABLE },
	{ "uappnd",	UF_APPEND },
	{ "opaque",	UF_OPAQUE },
	{ "uunlnk",	UF_NOUNLINK },
	{ "arch",	SF_ARCHIVED },
	{ "schg",	SF_IMMUTABLE },
	{ "sappnd",	SF_APPEND },
	{ "sunlnk",	SF_NOUNLINK },
	{ NULL, 0 },
};

unsigned int
tarfs_strtofflags(const char *str, char **end)
{
	const struct tarfs_flag *tf;
	const char *p, *q;
	unsigned int ret;

	ret = 0;
	for (p = q = str; *q != '\0'; p = q + 1) {
		for (q = p; *q != '\0' && *q != ','; ++q) {
			if (*q < 'a' || *q > 'z') {
				goto end;
			}
			/* nothing */
		}
		for (tf = tarfs_flags; tf->name != NULL; tf++) {
			if (strncmp(tf->name, p, q - p) == 0 &&
			    tf->name[q - p] == '\0') {
				TARFS_DPF(ALLOC, "%s: %.*s = 0x%06x\n", __func__,
				    (int)(q - p), p, tf->flag);
				ret |= tf->flag;
				break;
			}
		}
		if (tf->name == NULL) {
			TARFS_DPF(ALLOC, "%s: %.*s = 0x??????\n",
			    __func__, (int)(q - p), p);
			goto end;
		}
	}
end:
	if (*end != NULL) {
		*end = __DECONST(char *, q);
	}
	return (ret);
}
