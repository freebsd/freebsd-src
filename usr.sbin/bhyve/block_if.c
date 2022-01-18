/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013  Peter Grehan <grehan@freebsd.org>
 * All rights reserved.
 * Copyright 2020 Joyent, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/queue.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#include <assert.h>
#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <sysexits.h>
#include <unistd.h>

#include <machine/atomic.h>
#include <machine/vmm_snapshot.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "mevent.h"
#include "pci_emul.h"
#include "block_if.h"

#define BLOCKIF_SIG	0xb109b109

#define BLOCKIF_NUMTHR	8
#define BLOCKIF_MAXREQ	(BLOCKIF_RING_MAX + BLOCKIF_NUMTHR)

enum blockop {
	BOP_READ,
	BOP_WRITE,
	BOP_FLUSH,
	BOP_DELETE
};

enum blockstat {
	BST_FREE,
	BST_BLOCK,
	BST_PEND,
	BST_BUSY,
	BST_DONE
};

struct blockif_elem {
	TAILQ_ENTRY(blockif_elem) be_link;
	struct blockif_req  *be_req;
	enum blockop	     be_op;
	enum blockstat	     be_status;
	pthread_t            be_tid;
	off_t		     be_block;
};

struct blockif_ctxt {
	int			bc_magic;
	int			bc_fd;
	int			bc_ischr;
	int			bc_isgeom;
	int			bc_candelete;
	int			bc_rdonly;
	off_t			bc_size;
	int			bc_sectsz;
	int			bc_psectsz;
	int			bc_psectoff;
	int			bc_closing;
	int			bc_paused;
	int			bc_work_count;
	pthread_t		bc_btid[BLOCKIF_NUMTHR];
	pthread_mutex_t		bc_mtx;
	pthread_cond_t		bc_cond;
	pthread_cond_t		bc_paused_cond;
	pthread_cond_t		bc_work_done_cond;
	blockif_resize_cb	*bc_resize_cb;
	void			*bc_resize_cb_arg;
	struct mevent		*bc_resize_event;

	/* Request elements and free/pending/busy queues */
	TAILQ_HEAD(, blockif_elem) bc_freeq;
	TAILQ_HEAD(, blockif_elem) bc_pendq;
	TAILQ_HEAD(, blockif_elem) bc_busyq;
	struct blockif_elem	bc_reqs[BLOCKIF_MAXREQ];
};

static pthread_once_t blockif_once = PTHREAD_ONCE_INIT;

struct blockif_sig_elem {
	pthread_mutex_t			bse_mtx;
	pthread_cond_t			bse_cond;
	int				bse_pending;
	struct blockif_sig_elem		*bse_next;
};

static struct blockif_sig_elem *blockif_bse_head;

static int
blockif_enqueue(struct blockif_ctxt *bc, struct blockif_req *breq,
		enum blockop op)
{
	struct blockif_elem *be, *tbe;
	off_t off;
	int i;

	be = TAILQ_FIRST(&bc->bc_freeq);
	assert(be != NULL);
	assert(be->be_status == BST_FREE);
	TAILQ_REMOVE(&bc->bc_freeq, be, be_link);
	be->be_req = breq;
	be->be_op = op;
	switch (op) {
	case BOP_READ:
	case BOP_WRITE:
	case BOP_DELETE:
		off = breq->br_offset;
		for (i = 0; i < breq->br_iovcnt; i++)
			off += breq->br_iov[i].iov_len;
		break;
	default:
		off = OFF_MAX;
	}
	be->be_block = off;
	TAILQ_FOREACH(tbe, &bc->bc_pendq, be_link) {
		if (tbe->be_block == breq->br_offset)
			break;
	}
	if (tbe == NULL) {
		TAILQ_FOREACH(tbe, &bc->bc_busyq, be_link) {
			if (tbe->be_block == breq->br_offset)
				break;
		}
	}
	if (tbe == NULL)
		be->be_status = BST_PEND;
	else
		be->be_status = BST_BLOCK;
	TAILQ_INSERT_TAIL(&bc->bc_pendq, be, be_link);
	return (be->be_status == BST_PEND);
}

static int
blockif_dequeue(struct blockif_ctxt *bc, pthread_t t, struct blockif_elem **bep)
{
	struct blockif_elem *be;

	TAILQ_FOREACH(be, &bc->bc_pendq, be_link) {
		if (be->be_status == BST_PEND)
			break;
		assert(be->be_status == BST_BLOCK);
	}
	if (be == NULL)
		return (0);
	TAILQ_REMOVE(&bc->bc_pendq, be, be_link);
	be->be_status = BST_BUSY;
	be->be_tid = t;
	TAILQ_INSERT_TAIL(&bc->bc_busyq, be, be_link);
	*bep = be;
	return (1);
}

static void
blockif_complete(struct blockif_ctxt *bc, struct blockif_elem *be)
{
	struct blockif_elem *tbe;

	if (be->be_status == BST_DONE || be->be_status == BST_BUSY)
		TAILQ_REMOVE(&bc->bc_busyq, be, be_link);
	else
		TAILQ_REMOVE(&bc->bc_pendq, be, be_link);
	TAILQ_FOREACH(tbe, &bc->bc_pendq, be_link) {
		if (tbe->be_req->br_offset == be->be_block)
			tbe->be_status = BST_PEND;
	}
	be->be_tid = 0;
	be->be_status = BST_FREE;
	be->be_req = NULL;
	TAILQ_INSERT_TAIL(&bc->bc_freeq, be, be_link);
}

static int
blockif_flush_bc(struct blockif_ctxt *bc)
{
	if (bc->bc_ischr) {
		if (ioctl(bc->bc_fd, DIOCGFLUSH))
			return (errno);
	} else if (fsync(bc->bc_fd))
		return (errno);

	return (0);
}

static void
blockif_proc(struct blockif_ctxt *bc, struct blockif_elem *be, uint8_t *buf)
{
	struct blockif_req *br;
	off_t arg[2];
	ssize_t clen, len, off, boff, voff;
	int i, err;
	struct spacectl_range range;

	br = be->be_req;
	if (br->br_iovcnt <= 1)
		buf = NULL;
	err = 0;
	switch (be->be_op) {
	case BOP_READ:
		if (buf == NULL) {
			if ((len = preadv(bc->bc_fd, br->br_iov, br->br_iovcnt,
				   br->br_offset)) < 0)
				err = errno;
			else
				br->br_resid -= len;
			break;
		}
		i = 0;
		off = voff = 0;
		while (br->br_resid > 0) {
			len = MIN(br->br_resid, MAXPHYS);
			if (pread(bc->bc_fd, buf, len, br->br_offset +
			    off) < 0) {
				err = errno;
				break;
			}
			boff = 0;
			do {
				clen = MIN(len - boff, br->br_iov[i].iov_len -
				    voff);
				memcpy(br->br_iov[i].iov_base + voff,
				    buf + boff, clen);
				if (clen < br->br_iov[i].iov_len - voff)
					voff += clen;
				else {
					i++;
					voff = 0;
				}
				boff += clen;
			} while (boff < len);
			off += len;
			br->br_resid -= len;
		}
		break;
	case BOP_WRITE:
		if (bc->bc_rdonly) {
			err = EROFS;
			break;
		}
		if (buf == NULL) {
			if ((len = pwritev(bc->bc_fd, br->br_iov, br->br_iovcnt,
				    br->br_offset)) < 0)
				err = errno;
			else
				br->br_resid -= len;
			break;
		}
		i = 0;
		off = voff = 0;
		while (br->br_resid > 0) {
			len = MIN(br->br_resid, MAXPHYS);
			boff = 0;
			do {
				clen = MIN(len - boff, br->br_iov[i].iov_len -
				    voff);
				memcpy(buf + boff,
				    br->br_iov[i].iov_base + voff, clen);
				if (clen < br->br_iov[i].iov_len - voff)
					voff += clen;
				else {
					i++;
					voff = 0;
				}
				boff += clen;
			} while (boff < len);
			if (pwrite(bc->bc_fd, buf, len, br->br_offset +
			    off) < 0) {
				err = errno;
				break;
			}
			off += len;
			br->br_resid -= len;
		}
		break;
	case BOP_FLUSH:
		err = blockif_flush_bc(bc);
		break;
	case BOP_DELETE:
		if (!bc->bc_candelete)
			err = EOPNOTSUPP;
		else if (bc->bc_rdonly)
			err = EROFS;
		else if (bc->bc_ischr) {
			arg[0] = br->br_offset;
			arg[1] = br->br_resid;
			if (ioctl(bc->bc_fd, DIOCGDELETE, arg))
				err = errno;
			else
				br->br_resid = 0;
		} else {
			range.r_offset = br->br_offset;
			range.r_len = br->br_resid;

			while (range.r_len > 0) {
				if (fspacectl(bc->bc_fd, SPACECTL_DEALLOC,
				    &range, 0, &range) != 0) {
					err = errno;
					break;
				}
			}
			if (err == 0)
				br->br_resid = 0;
		}
		break;
	default:
		err = EINVAL;
		break;
	}

	be->be_status = BST_DONE;

	(*br->br_callback)(br, err);
}

static void *
blockif_thr(void *arg)
{
	struct blockif_ctxt *bc;
	struct blockif_elem *be;
	pthread_t t;
	uint8_t *buf;

	bc = arg;
	if (bc->bc_isgeom)
		buf = malloc(MAXPHYS);
	else
		buf = NULL;
	t = pthread_self();

	pthread_mutex_lock(&bc->bc_mtx);
	for (;;) {
		bc->bc_work_count++;

		/* We cannot process work if the interface is paused */
		while (!bc->bc_paused && blockif_dequeue(bc, t, &be)) {
			pthread_mutex_unlock(&bc->bc_mtx);
			blockif_proc(bc, be, buf);
			pthread_mutex_lock(&bc->bc_mtx);
			blockif_complete(bc, be);
		}

		bc->bc_work_count--;

		/* If none of the workers are busy, notify the main thread */
		if (bc->bc_work_count == 0)
			pthread_cond_broadcast(&bc->bc_work_done_cond);

		/* Check ctxt status here to see if exit requested */
		if (bc->bc_closing)
			break;

		/* Make all worker threads wait here if the device is paused */
		while (bc->bc_paused)
			pthread_cond_wait(&bc->bc_paused_cond, &bc->bc_mtx);

		pthread_cond_wait(&bc->bc_cond, &bc->bc_mtx);
	}
	pthread_mutex_unlock(&bc->bc_mtx);

	if (buf)
		free(buf);
	pthread_exit(NULL);
	return (NULL);
}

static void
blockif_sigcont_handler(int signal, enum ev_type type, void *arg)
{
	struct blockif_sig_elem *bse;

	for (;;) {
		/*
		 * Process the entire list even if not intended for
		 * this thread.
		 */
		do {
			bse = blockif_bse_head;
			if (bse == NULL)
				return;
		} while (!atomic_cmpset_ptr((uintptr_t *)&blockif_bse_head,
					    (uintptr_t)bse,
					    (uintptr_t)bse->bse_next));

		pthread_mutex_lock(&bse->bse_mtx);
		bse->bse_pending = 0;
		pthread_cond_signal(&bse->bse_cond);
		pthread_mutex_unlock(&bse->bse_mtx);
	}
}

static void
blockif_init(void)
{
	mevent_add(SIGCONT, EVF_SIGNAL, blockif_sigcont_handler, NULL);
	(void) signal(SIGCONT, SIG_IGN);
}

int
blockif_legacy_config(nvlist_t *nvl, const char *opts)
{
	char *cp, *path;

	if (opts == NULL)
		return (0);

	cp = strchr(opts, ',');
	if (cp == NULL) {
		set_config_value_node(nvl, "path", opts);
		return (0);
	}
	path = strndup(opts, cp - opts);
	set_config_value_node(nvl, "path", path);
	free(path);
	return (pci_parse_legacy_config(nvl, cp + 1));
}

struct blockif_ctxt *
blockif_open(nvlist_t *nvl, const char *ident)
{
	char tname[MAXCOMLEN + 1];
	char name[MAXPATHLEN];
	const char *path, *pssval, *ssval;
	char *cp;
	struct blockif_ctxt *bc;
	struct stat sbuf;
	struct diocgattr_arg arg;
	off_t size, psectsz, psectoff;
	int extra, fd, i, sectsz;
	int ro, candelete, geom, ssopt, pssopt;
	int nodelete;

#ifndef WITHOUT_CAPSICUM
	cap_rights_t rights;
	cap_ioctl_t cmds[] = { DIOCGFLUSH, DIOCGDELETE };
#endif

	pthread_once(&blockif_once, blockif_init);

	fd = -1;
	extra = 0;
	ssopt = 0;
	ro = 0;
	nodelete = 0;

	if (get_config_bool_node_default(nvl, "nocache", false))
		extra |= O_DIRECT;
	if (get_config_bool_node_default(nvl, "nodelete", false))
		nodelete = 1;
	if (get_config_bool_node_default(nvl, "sync", false) ||
	    get_config_bool_node_default(nvl, "direct", false))
		extra |= O_SYNC;
	if (get_config_bool_node_default(nvl, "ro", false))
		ro = 1;
	ssval = get_config_value_node(nvl, "sectorsize");
	if (ssval != NULL) {
		ssopt = strtol(ssval, &cp, 10);
		if (cp == ssval) {
			EPRINTLN("Invalid sector size \"%s\"", ssval);
			goto err;
		}
		if (*cp == '\0') {
			pssopt = ssopt;
		} else if (*cp == '/') {
			pssval = cp + 1;
			pssopt = strtol(pssval, &cp, 10);
			if (cp == pssval || *cp != '\0') {
				EPRINTLN("Invalid sector size \"%s\"", ssval);
				goto err;
			}
		} else {
			EPRINTLN("Invalid sector size \"%s\"", ssval);
			goto err;
		}
	}

	path = get_config_value_node(nvl, "path");
	if (path == NULL) {
		EPRINTLN("Missing \"path\" for block device.");
		goto err;
	}

	fd = open(path, (ro ? O_RDONLY : O_RDWR) | extra);
	if (fd < 0 && !ro) {
		/* Attempt a r/w fail with a r/o open */
		fd = open(path, O_RDONLY | extra);
		ro = 1;
	}

	if (fd < 0) {
		warn("Could not open backing file: %s", path);
		goto err;
	}

        if (fstat(fd, &sbuf) < 0) {
		warn("Could not stat backing file %s", path);
		goto err;
        }

#ifndef WITHOUT_CAPSICUM
	cap_rights_init(&rights, CAP_FSYNC, CAP_IOCTL, CAP_READ, CAP_SEEK,
	    CAP_WRITE, CAP_FSTAT, CAP_EVENT, CAP_FPATHCONF);
	if (ro)
		cap_rights_clear(&rights, CAP_FSYNC, CAP_WRITE);

	if (caph_rights_limit(fd, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

        /*
	 * Deal with raw devices
	 */
        size = sbuf.st_size;
	sectsz = DEV_BSIZE;
	psectsz = psectoff = 0;
	candelete = geom = 0;
	if (S_ISCHR(sbuf.st_mode)) {
		if (ioctl(fd, DIOCGMEDIASIZE, &size) < 0 ||
		    ioctl(fd, DIOCGSECTORSIZE, &sectsz)) {
			perror("Could not fetch dev blk/sector size");
			goto err;
		}
		assert(size != 0);
		assert(sectsz != 0);
		if (ioctl(fd, DIOCGSTRIPESIZE, &psectsz) == 0 && psectsz > 0)
			ioctl(fd, DIOCGSTRIPEOFFSET, &psectoff);
		strlcpy(arg.name, "GEOM::candelete", sizeof(arg.name));
		arg.len = sizeof(arg.value.i);
		if (nodelete == 0 && ioctl(fd, DIOCGATTR, &arg) == 0)
			candelete = arg.value.i;
		if (ioctl(fd, DIOCGPROVIDERNAME, name) == 0)
			geom = 1;
	} else {
		psectsz = sbuf.st_blksize;
		/* Avoid fallback implementation */
		candelete = fpathconf(fd, _PC_DEALLOC_PRESENT) == 1;
	}

#ifndef WITHOUT_CAPSICUM
	if (caph_ioctls_limit(fd, cmds, nitems(cmds)) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
#endif

	if (ssopt != 0) {
		if (!powerof2(ssopt) || !powerof2(pssopt) || ssopt < 512 ||
		    ssopt > pssopt) {
			EPRINTLN("Invalid sector size %d/%d",
			    ssopt, pssopt);
			goto err;
		}

		/*
		 * Some backend drivers (e.g. cd0, ada0) require that the I/O
		 * size be a multiple of the device's sector size.
		 *
		 * Validate that the emulated sector size complies with this
		 * requirement.
		 */
		if (S_ISCHR(sbuf.st_mode)) {
			if (ssopt < sectsz || (ssopt % sectsz) != 0) {
				EPRINTLN("Sector size %d incompatible "
				    "with underlying device sector size %d",
				    ssopt, sectsz);
				goto err;
			}
		}

		sectsz = ssopt;
		psectsz = pssopt;
		psectoff = 0;
	}

	bc = calloc(1, sizeof(struct blockif_ctxt));
	if (bc == NULL) {
		perror("calloc");
		goto err;
	}

	bc->bc_magic = BLOCKIF_SIG;
	bc->bc_fd = fd;
	bc->bc_ischr = S_ISCHR(sbuf.st_mode);
	bc->bc_isgeom = geom;
	bc->bc_candelete = candelete;
	bc->bc_rdonly = ro;
	bc->bc_size = size;
	bc->bc_sectsz = sectsz;
	bc->bc_psectsz = psectsz;
	bc->bc_psectoff = psectoff;
	pthread_mutex_init(&bc->bc_mtx, NULL);
	pthread_cond_init(&bc->bc_cond, NULL);
	bc->bc_paused = 0;
	bc->bc_work_count = 0;
	pthread_cond_init(&bc->bc_paused_cond, NULL);
	pthread_cond_init(&bc->bc_work_done_cond, NULL);
	TAILQ_INIT(&bc->bc_freeq);
	TAILQ_INIT(&bc->bc_pendq);
	TAILQ_INIT(&bc->bc_busyq);
	for (i = 0; i < BLOCKIF_MAXREQ; i++) {
		bc->bc_reqs[i].be_status = BST_FREE;
		TAILQ_INSERT_HEAD(&bc->bc_freeq, &bc->bc_reqs[i], be_link);
	}

	for (i = 0; i < BLOCKIF_NUMTHR; i++) {
		pthread_create(&bc->bc_btid[i], NULL, blockif_thr, bc);
		snprintf(tname, sizeof(tname), "blk-%s-%d", ident, i);
		pthread_set_name_np(bc->bc_btid[i], tname);
	}

	return (bc);
err:
	if (fd >= 0)
		close(fd);
	return (NULL);
}

static void
blockif_resized(int fd, enum ev_type type, void *arg)
{
	struct blockif_ctxt *bc;
	struct stat sb;
	off_t mediasize;

	if (fstat(fd, &sb) != 0)
		return;

	if (S_ISCHR(sb.st_mode)) {
		if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) < 0) {
			EPRINTLN("blockif_resized: get mediasize failed: %s",
			    strerror(errno));
			return;
		}
	} else
		mediasize = sb.st_size;

	bc = arg;
	pthread_mutex_lock(&bc->bc_mtx);
	if (mediasize != bc->bc_size) {
		bc->bc_size = mediasize;
		bc->bc_resize_cb(bc, bc->bc_resize_cb_arg, bc->bc_size);
	}
	pthread_mutex_unlock(&bc->bc_mtx);
}

int
blockif_register_resize_callback(struct blockif_ctxt *bc, blockif_resize_cb *cb,
    void *cb_arg)
{
	struct stat sb;
	int err;

	if (cb == NULL)
		return (EINVAL);

	pthread_mutex_lock(&bc->bc_mtx);
	if (bc->bc_resize_cb != NULL) {
		err = EBUSY;
		goto out;
	}

	assert(bc->bc_closing == 0);

	if (fstat(bc->bc_fd, &sb) != 0) {
		err = errno;
		goto out;
	}

	bc->bc_resize_event = mevent_add_flags(bc->bc_fd, EVF_VNODE,
	    EVFF_ATTRIB, blockif_resized, bc);
	if (bc->bc_resize_event == NULL) {
		err = ENXIO;
		goto out;
	}

	bc->bc_resize_cb = cb;
	bc->bc_resize_cb_arg = cb_arg;
out:
	pthread_mutex_unlock(&bc->bc_mtx);

	return (err);
}

static int
blockif_request(struct blockif_ctxt *bc, struct blockif_req *breq,
		enum blockop op)
{
	int err;

	err = 0;

	pthread_mutex_lock(&bc->bc_mtx);
	if (!TAILQ_EMPTY(&bc->bc_freeq)) {
		/*
		 * Enqueue and inform the block i/o thread
		 * that there is work available
		 */
		if (blockif_enqueue(bc, breq, op))
			pthread_cond_signal(&bc->bc_cond);
	} else {
		/*
		 * Callers are not allowed to enqueue more than
		 * the specified blockif queue limit. Return an
		 * error to indicate that the queue length has been
		 * exceeded.
		 */
		err = E2BIG;
	}
	pthread_mutex_unlock(&bc->bc_mtx);

	return (err);
}

int
blockif_read(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_READ));
}

int
blockif_write(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_WRITE));
}

int
blockif_flush(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_FLUSH));
}

int
blockif_delete(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_DELETE));
}

int
blockif_cancel(struct blockif_ctxt *bc, struct blockif_req *breq)
{
	struct blockif_elem *be;

	assert(bc->bc_magic == BLOCKIF_SIG);

	pthread_mutex_lock(&bc->bc_mtx);
	/* XXX: not waiting while paused */

	/*
	 * Check pending requests.
	 */
	TAILQ_FOREACH(be, &bc->bc_pendq, be_link) {
		if (be->be_req == breq)
			break;
	}
	if (be != NULL) {
		/*
		 * Found it.
		 */
		blockif_complete(bc, be);
		pthread_mutex_unlock(&bc->bc_mtx);

		return (0);
	}

	/*
	 * Check in-flight requests.
	 */
	TAILQ_FOREACH(be, &bc->bc_busyq, be_link) {
		if (be->be_req == breq)
			break;
	}
	if (be == NULL) {
		/*
		 * Didn't find it.
		 */
		pthread_mutex_unlock(&bc->bc_mtx);
		return (EINVAL);
	}

	/*
	 * Interrupt the processing thread to force it return
	 * prematurely via it's normal callback path.
	 */
	while (be->be_status == BST_BUSY) {
		struct blockif_sig_elem bse, *old_head;

		pthread_mutex_init(&bse.bse_mtx, NULL);
		pthread_cond_init(&bse.bse_cond, NULL);

		bse.bse_pending = 1;

		do {
			old_head = blockif_bse_head;
			bse.bse_next = old_head;
		} while (!atomic_cmpset_ptr((uintptr_t *)&blockif_bse_head,
					    (uintptr_t)old_head,
					    (uintptr_t)&bse));

		pthread_kill(be->be_tid, SIGCONT);

		pthread_mutex_lock(&bse.bse_mtx);
		while (bse.bse_pending)
			pthread_cond_wait(&bse.bse_cond, &bse.bse_mtx);
		pthread_mutex_unlock(&bse.bse_mtx);
	}

	pthread_mutex_unlock(&bc->bc_mtx);

	/*
	 * The processing thread has been interrupted.  Since it's not
	 * clear if the callback has been invoked yet, return EBUSY.
	 */
	return (EBUSY);
}

int
blockif_close(struct blockif_ctxt *bc)
{
	void *jval;
	int i;

	assert(bc->bc_magic == BLOCKIF_SIG);

	/*
	 * Stop the block i/o thread
	 */
	pthread_mutex_lock(&bc->bc_mtx);
	bc->bc_closing = 1;
	if (bc->bc_resize_event != NULL)
		mevent_disable(bc->bc_resize_event);
	pthread_mutex_unlock(&bc->bc_mtx);
	pthread_cond_broadcast(&bc->bc_cond);
	for (i = 0; i < BLOCKIF_NUMTHR; i++)
		pthread_join(bc->bc_btid[i], &jval);

	/* XXX Cancel queued i/o's ??? */

	/*
	 * Release resources
	 */
	bc->bc_magic = 0;
	close(bc->bc_fd);
	free(bc);

	return (0);
}

/*
 * Return virtual C/H/S values for a given block. Use the algorithm
 * outlined in the VHD specification to calculate values.
 */
void
blockif_chs(struct blockif_ctxt *bc, uint16_t *c, uint8_t *h, uint8_t *s)
{
	off_t sectors;		/* total sectors of the block dev */
	off_t hcyl;		/* cylinders times heads */
	uint16_t secpt;		/* sectors per track */
	uint8_t heads;

	assert(bc->bc_magic == BLOCKIF_SIG);

	sectors = bc->bc_size / bc->bc_sectsz;

	/* Clamp the size to the largest possible with CHS */
	if (sectors > 65535UL*16*255)
		sectors = 65535UL*16*255;

	if (sectors >= 65536UL*16*63) {
		secpt = 255;
		heads = 16;
		hcyl = sectors / secpt;
	} else {
		secpt = 17;
		hcyl = sectors / secpt;
		heads = (hcyl + 1023) / 1024;

		if (heads < 4)
			heads = 4;

		if (hcyl >= (heads * 1024) || heads > 16) {
			secpt = 31;
			heads = 16;
			hcyl = sectors / secpt;
		}
		if (hcyl >= (heads * 1024)) {
			secpt = 63;
			heads = 16;
			hcyl = sectors / secpt;
		}
	}

	*c = hcyl / heads;
	*h = heads;
	*s = secpt;
}

/*
 * Accessors
 */
off_t
blockif_size(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_size);
}

int
blockif_sectsz(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_sectsz);
}

void
blockif_psectsz(struct blockif_ctxt *bc, int *size, int *off)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	*size = bc->bc_psectsz;
	*off = bc->bc_psectoff;
}

int
blockif_queuesz(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (BLOCKIF_MAXREQ - 1);
}

int
blockif_is_ro(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_rdonly);
}

int
blockif_candelete(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_candelete);
}

#ifdef BHYVE_SNAPSHOT
void
blockif_pause(struct blockif_ctxt *bc)
{
	assert(bc != NULL);
	assert(bc->bc_magic == BLOCKIF_SIG);

	pthread_mutex_lock(&bc->bc_mtx);
	bc->bc_paused = 1;

	/* The interface is paused. Wait for workers to finish their work */
	while (bc->bc_work_count)
		pthread_cond_wait(&bc->bc_work_done_cond, &bc->bc_mtx);
	pthread_mutex_unlock(&bc->bc_mtx);

	if (blockif_flush_bc(bc))
		fprintf(stderr, "%s: [WARN] failed to flush backing file.\r\n",
			__func__);
}

void
blockif_resume(struct blockif_ctxt *bc)
{
	assert(bc != NULL);
	assert(bc->bc_magic == BLOCKIF_SIG);

	pthread_mutex_lock(&bc->bc_mtx);
	bc->bc_paused = 0;
	/* resume the threads waiting for paused */
	pthread_cond_broadcast(&bc->bc_paused_cond);
	/* kick the threads after restore */
	pthread_cond_broadcast(&bc->bc_cond);
	pthread_mutex_unlock(&bc->bc_mtx);
}

int
blockif_snapshot_req(struct blockif_req *br, struct vm_snapshot_meta *meta)
{
	int i;
	struct iovec *iov;
	int ret;

	SNAPSHOT_VAR_OR_LEAVE(br->br_iovcnt, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(br->br_offset, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(br->br_resid, meta, ret, done);

	/*
	 * XXX: The callback and parameter must be filled by the virtualized
	 * device that uses the interface, during its init; we're not touching
	 * them here.
	 */

	/* Snapshot the iovecs. */
	for (i = 0; i < br->br_iovcnt; i++) {
		iov = &br->br_iov[i];

		SNAPSHOT_VAR_OR_LEAVE(iov->iov_len, meta, ret, done);

		/* We assume the iov is a guest-mapped address. */
		SNAPSHOT_GUEST2HOST_ADDR_OR_LEAVE(iov->iov_base, iov->iov_len,
			false, meta, ret, done);
	}

done:
	return (ret);
}

int
blockif_snapshot(struct blockif_ctxt *bc, struct vm_snapshot_meta *meta)
{
	int ret;

	if (bc->bc_paused == 0) {
		fprintf(stderr, "%s: Snapshot failed: "
			"interface not paused.\r\n", __func__);
		return (ENXIO);
	}

	pthread_mutex_lock(&bc->bc_mtx);

	SNAPSHOT_VAR_OR_LEAVE(bc->bc_magic, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_ischr, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_isgeom, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_candelete, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_rdonly, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_size, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_sectsz, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_psectsz, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_psectoff, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(bc->bc_closing, meta, ret, done);

done:
	pthread_mutex_unlock(&bc->bc_mtx);
	return (ret);
}
#endif
