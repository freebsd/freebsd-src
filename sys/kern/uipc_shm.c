/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006, 2011, 2016-2017 Robert N. M. Watson
 * Copyright 2020 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by BAE Systems, the University of
 * Cambridge Computer Laboratory, and Memorial University under DARPA/AFRL
 * contract FA8650-15-C-7558 ("CADETS"), as part of the DARPA Transparent
 * Computing (TC) research program.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * Support for shared swap-backed anonymous memory objects via
 * shm_open(2), shm_rename(2), and shm_unlink(2).
 * While most of the implementation is here, vm_mmap.c contains
 * mapping logic changes.
 *
 * posixshmcontrol(1) allows users to inspect the state of the memory
 * objects.  Per-uid swap resource limit controls total amount of
 * memory that user can consume for anonymous objects, including
 * shared.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_capsicum.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/fnv_hash.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/uio.h>
#include <sys/signal.h>
#include <sys/jail.h>
#include <sys/ktrace.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/time.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/unistd.h>
#include <sys/user.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

struct shm_mapping {
	char		*sm_path;
	Fnv32_t		sm_fnv;
	struct shmfd	*sm_shmfd;
	LIST_ENTRY(shm_mapping) sm_link;
};

static MALLOC_DEFINE(M_SHMFD, "shmfd", "shared memory file descriptor");
static LIST_HEAD(, shm_mapping) *shm_dictionary;
static struct sx shm_dict_lock;
static struct mtx shm_timestamp_lock;
static u_long shm_hash;
static struct unrhdr64 shm_ino_unr;
static dev_t shm_dev_ino;

#define	SHM_HASH(fnv)	(&shm_dictionary[(fnv) & shm_hash])

static void	shm_init(void *arg);
static void	shm_insert(char *path, Fnv32_t fnv, struct shmfd *shmfd);
static struct shmfd *shm_lookup(char *path, Fnv32_t fnv);
static int	shm_remove(char *path, Fnv32_t fnv, struct ucred *ucred);
static int	shm_dotruncate_cookie(struct shmfd *shmfd, off_t length,
    void *rl_cookie);
static int	shm_dotruncate_locked(struct shmfd *shmfd, off_t length,
    void *rl_cookie);
static int	shm_copyin_path(struct thread *td, const char *userpath_in,
    char **path_out);
static int	shm_deallocate(struct shmfd *shmfd, off_t *offset,
    off_t *length, int flags);

static fo_rdwr_t	shm_read;
static fo_rdwr_t	shm_write;
static fo_truncate_t	shm_truncate;
static fo_ioctl_t	shm_ioctl;
static fo_stat_t	shm_stat;
static fo_close_t	shm_close;
static fo_chmod_t	shm_chmod;
static fo_chown_t	shm_chown;
static fo_seek_t	shm_seek;
static fo_fill_kinfo_t	shm_fill_kinfo;
static fo_mmap_t	shm_mmap;
static fo_get_seals_t	shm_get_seals;
static fo_add_seals_t	shm_add_seals;
static fo_fallocate_t	shm_fallocate;
static fo_fspacectl_t	shm_fspacectl;

/* File descriptor operations. */
struct fileops shm_ops = {
	.fo_read = shm_read,
	.fo_write = shm_write,
	.fo_truncate = shm_truncate,
	.fo_ioctl = shm_ioctl,
	.fo_poll = invfo_poll,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = shm_stat,
	.fo_close = shm_close,
	.fo_chmod = shm_chmod,
	.fo_chown = shm_chown,
	.fo_sendfile = vn_sendfile,
	.fo_seek = shm_seek,
	.fo_fill_kinfo = shm_fill_kinfo,
	.fo_mmap = shm_mmap,
	.fo_get_seals = shm_get_seals,
	.fo_add_seals = shm_add_seals,
	.fo_fallocate = shm_fallocate,
	.fo_fspacectl = shm_fspacectl,
	.fo_flags = DFLAG_PASSABLE | DFLAG_SEEKABLE,
};

FEATURE(posix_shm, "POSIX shared memory");

static SYSCTL_NODE(_vm, OID_AUTO, largepages, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "");

static int largepage_reclaim_tries = 1;
SYSCTL_INT(_vm_largepages, OID_AUTO, reclaim_tries,
    CTLFLAG_RWTUN, &largepage_reclaim_tries, 0,
    "Number of contig reclaims before giving up for default alloc policy");

static int
uiomove_object_page(vm_object_t obj, size_t len, struct uio *uio)
{
	vm_page_t m;
	vm_pindex_t idx;
	size_t tlen;
	int error, offset, rv;

	idx = OFF_TO_IDX(uio->uio_offset);
	offset = uio->uio_offset & PAGE_MASK;
	tlen = MIN(PAGE_SIZE - offset, len);

	rv = vm_page_grab_valid_unlocked(&m, obj, idx,
	    VM_ALLOC_SBUSY | VM_ALLOC_IGN_SBUSY | VM_ALLOC_NOCREAT);
	if (rv == VM_PAGER_OK)
		goto found;

	/*
	 * Read I/O without either a corresponding resident page or swap
	 * page: use zero_region.  This is intended to avoid instantiating
	 * pages on read from a sparse region.
	 */
	VM_OBJECT_WLOCK(obj);
	m = vm_page_lookup(obj, idx);
	if (uio->uio_rw == UIO_READ && m == NULL &&
	    !vm_pager_has_page(obj, idx, NULL, NULL)) {
		VM_OBJECT_WUNLOCK(obj);
		return (uiomove(__DECONST(void *, zero_region), tlen, uio));
	}

	/*
	 * Although the tmpfs vnode lock is held here, it is
	 * nonetheless safe to sleep waiting for a free page.  The
	 * pageout daemon does not need to acquire the tmpfs vnode
	 * lock to page out tobj's pages because tobj is a OBJT_SWAP
	 * type object.
	 */
	rv = vm_page_grab_valid(&m, obj, idx,
	    VM_ALLOC_NORMAL | VM_ALLOC_SBUSY | VM_ALLOC_IGN_SBUSY);
	if (rv != VM_PAGER_OK) {
		VM_OBJECT_WUNLOCK(obj);
		printf("uiomove_object: vm_obj %p idx %jd pager error %d\n",
		    obj, idx, rv);
		return (EIO);
	}
	VM_OBJECT_WUNLOCK(obj);

found:
	error = uiomove_fromphys(&m, offset, tlen, uio);
	if (uio->uio_rw == UIO_WRITE && error == 0)
		vm_page_set_dirty(m);
	vm_page_activate(m);
	vm_page_sunbusy(m);

	return (error);
}

int
uiomove_object(vm_object_t obj, off_t obj_size, struct uio *uio)
{
	ssize_t resid;
	size_t len;
	int error;

	error = 0;
	while ((resid = uio->uio_resid) > 0) {
		if (obj_size <= uio->uio_offset)
			break;
		len = MIN(obj_size - uio->uio_offset, resid);
		if (len == 0)
			break;
		error = uiomove_object_page(obj, len, uio);
		if (error != 0 || resid == uio->uio_resid)
			break;
	}
	return (error);
}

static u_long count_largepages[MAXPAGESIZES];

static int
shm_largepage_phys_populate(vm_object_t object, vm_pindex_t pidx,
    int fault_type, vm_prot_t max_prot, vm_pindex_t *first, vm_pindex_t *last)
{
	vm_page_t m __diagused;
	int psind;

	psind = object->un_pager.phys.data_val;
	if (psind == 0 || pidx >= object->size)
		return (VM_PAGER_FAIL);
	*first = rounddown2(pidx, pagesizes[psind] / PAGE_SIZE);

	/*
	 * We only busy the first page in the superpage run.  It is
	 * useless to busy whole run since we only remove full
	 * superpage, and it takes too long to busy e.g. 512 * 512 ==
	 * 262144 pages constituing 1G amd64 superage.
	 */
	m = vm_page_grab(object, *first, VM_ALLOC_NORMAL | VM_ALLOC_NOCREAT);
	MPASS(m != NULL);

	*last = *first + atop(pagesizes[psind]) - 1;
	return (VM_PAGER_OK);
}

static boolean_t
shm_largepage_phys_haspage(vm_object_t object, vm_pindex_t pindex,
    int *before, int *after)
{
	int psind;

	psind = object->un_pager.phys.data_val;
	if (psind == 0 || pindex >= object->size)
		return (FALSE);
	if (before != NULL) {
		*before = pindex - rounddown2(pindex, pagesizes[psind] /
		    PAGE_SIZE);
	}
	if (after != NULL) {
		*after = roundup2(pindex, pagesizes[psind] / PAGE_SIZE) -
		    pindex;
	}
	return (TRUE);
}

static void
shm_largepage_phys_ctor(vm_object_t object, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred)
{
}

static void
shm_largepage_phys_dtor(vm_object_t object)
{
	int psind;

	psind = object->un_pager.phys.data_val;
	if (psind != 0) {
		atomic_subtract_long(&count_largepages[psind],
		    object->size / (pagesizes[psind] / PAGE_SIZE));
		vm_wire_sub(object->size);
	} else {
		KASSERT(object->size == 0,
		    ("largepage phys obj %p not initialized bit size %#jx > 0",
		    object, (uintmax_t)object->size));
	}
}

static const struct phys_pager_ops shm_largepage_phys_ops = {
	.phys_pg_populate =	shm_largepage_phys_populate,
	.phys_pg_haspage =	shm_largepage_phys_haspage,
	.phys_pg_ctor =		shm_largepage_phys_ctor,
	.phys_pg_dtor =		shm_largepage_phys_dtor,
};

bool
shm_largepage(struct shmfd *shmfd)
{
	return (shmfd->shm_object->type == OBJT_PHYS);
}

static int
shm_seek(struct file *fp, off_t offset, int whence, struct thread *td)
{
	struct shmfd *shmfd;
	off_t foffset;
	int error;

	shmfd = fp->f_data;
	foffset = foffset_lock(fp, 0);
	error = 0;
	switch (whence) {
	case L_INCR:
		if (foffset < 0 ||
		    (offset > 0 && foffset > OFF_MAX - offset)) {
			error = EOVERFLOW;
			break;
		}
		offset += foffset;
		break;
	case L_XTND:
		if (offset > 0 && shmfd->shm_size > OFF_MAX - offset) {
			error = EOVERFLOW;
			break;
		}
		offset += shmfd->shm_size;
		break;
	case L_SET:
		break;
	default:
		error = EINVAL;
	}
	if (error == 0) {
		if (offset < 0 || offset > shmfd->shm_size)
			error = EINVAL;
		else
			td->td_uretoff.tdu_off = offset;
	}
	foffset_unlock(fp, offset, error != 0 ? FOF_NOUPDATE : 0);
	return (error);
}

static int
shm_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct shmfd *shmfd;
	void *rl_cookie;
	int error;

	shmfd = fp->f_data;
#ifdef MAC
	error = mac_posixshm_check_read(active_cred, fp->f_cred, shmfd);
	if (error)
		return (error);
#endif
	foffset_lock_uio(fp, uio, flags);
	rl_cookie = rangelock_rlock(&shmfd->shm_rl, uio->uio_offset,
	    uio->uio_offset + uio->uio_resid, &shmfd->shm_mtx);
	error = uiomove_object(shmfd->shm_object, shmfd->shm_size, uio);
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	foffset_unlock_uio(fp, uio, flags);
	return (error);
}

static int
shm_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct shmfd *shmfd;
	void *rl_cookie;
	int error;
	off_t size;

	shmfd = fp->f_data;
#ifdef MAC
	error = mac_posixshm_check_write(active_cred, fp->f_cred, shmfd);
	if (error)
		return (error);
#endif
	if (shm_largepage(shmfd) && shmfd->shm_lp_psind == 0)
		return (EINVAL);
	foffset_lock_uio(fp, uio, flags);
	if (uio->uio_resid > OFF_MAX - uio->uio_offset) {
		/*
		 * Overflow is only an error if we're supposed to expand on
		 * write.  Otherwise, we'll just truncate the write to the
		 * size of the file, which can only grow up to OFF_MAX.
		 */
		if ((shmfd->shm_flags & SHM_GROW_ON_WRITE) != 0) {
			foffset_unlock_uio(fp, uio, flags);
			return (EFBIG);
		}

		size = shmfd->shm_size;
	} else {
		size = uio->uio_offset + uio->uio_resid;
	}
	if ((flags & FOF_OFFSET) == 0) {
		rl_cookie = rangelock_wlock(&shmfd->shm_rl, 0, OFF_MAX,
		    &shmfd->shm_mtx);
	} else {
		rl_cookie = rangelock_wlock(&shmfd->shm_rl, uio->uio_offset,
		    size, &shmfd->shm_mtx);
	}
	if ((shmfd->shm_seals & F_SEAL_WRITE) != 0) {
		error = EPERM;
	} else {
		error = 0;
		if ((shmfd->shm_flags & SHM_GROW_ON_WRITE) != 0 &&
		    size > shmfd->shm_size) {
			error = shm_dotruncate_cookie(shmfd, size, rl_cookie);
		}
		if (error == 0)
			error = uiomove_object(shmfd->shm_object,
			    shmfd->shm_size, uio);
	}
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	foffset_unlock_uio(fp, uio, flags);
	return (error);
}

static int
shm_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{
	struct shmfd *shmfd;
#ifdef MAC
	int error;
#endif

	shmfd = fp->f_data;
#ifdef MAC
	error = mac_posixshm_check_truncate(active_cred, fp->f_cred, shmfd);
	if (error)
		return (error);
#endif
	return (shm_dotruncate(shmfd, length));
}

int
shm_ioctl(struct file *fp, u_long com, void *data, struct ucred *active_cred,
    struct thread *td)
{
	struct shmfd *shmfd;
	struct shm_largepage_conf *conf;
	void *rl_cookie;

	shmfd = fp->f_data;
	switch (com) {
	case FIONBIO:
	case FIOASYNC:
		/*
		 * Allow fcntl(fd, F_SETFL, O_NONBLOCK) to work,
		 * just like it would on an unlinked regular file
		 */
		return (0);
	case FIOSSHMLPGCNF:
		if (!shm_largepage(shmfd))
			return (ENOTTY);
		conf = data;
		if (shmfd->shm_lp_psind != 0 &&
		    conf->psind != shmfd->shm_lp_psind)
			return (EINVAL);
		if (conf->psind <= 0 || conf->psind >= MAXPAGESIZES ||
		    pagesizes[conf->psind] == 0)
			return (EINVAL);
		if (conf->alloc_policy != SHM_LARGEPAGE_ALLOC_DEFAULT &&
		    conf->alloc_policy != SHM_LARGEPAGE_ALLOC_NOWAIT &&
		    conf->alloc_policy != SHM_LARGEPAGE_ALLOC_HARD)
			return (EINVAL);

		rl_cookie = rangelock_wlock(&shmfd->shm_rl, 0, OFF_MAX,
		    &shmfd->shm_mtx);
		shmfd->shm_lp_psind = conf->psind;
		shmfd->shm_lp_alloc_policy = conf->alloc_policy;
		shmfd->shm_object->un_pager.phys.data_val = conf->psind;
		rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
		return (0);
	case FIOGSHMLPGCNF:
		if (!shm_largepage(shmfd))
			return (ENOTTY);
		conf = data;
		rl_cookie = rangelock_rlock(&shmfd->shm_rl, 0, OFF_MAX,
		    &shmfd->shm_mtx);
		conf->psind = shmfd->shm_lp_psind;
		conf->alloc_policy = shmfd->shm_lp_alloc_policy;
		rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
		return (0);
	default:
		return (ENOTTY);
	}
}

static int
shm_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{
	struct shmfd *shmfd;
#ifdef MAC
	int error;
#endif

	shmfd = fp->f_data;

#ifdef MAC
	error = mac_posixshm_check_stat(active_cred, fp->f_cred, shmfd);
	if (error)
		return (error);
#endif

	/*
	 * Attempt to return sanish values for fstat() on a memory file
	 * descriptor.
	 */
	bzero(sb, sizeof(*sb));
	sb->st_blksize = PAGE_SIZE;
	sb->st_size = shmfd->shm_size;
	sb->st_blocks = howmany(sb->st_size, sb->st_blksize);
	mtx_lock(&shm_timestamp_lock);
	sb->st_atim = shmfd->shm_atime;
	sb->st_ctim = shmfd->shm_ctime;
	sb->st_mtim = shmfd->shm_mtime;
	sb->st_birthtim = shmfd->shm_birthtime;
	sb->st_mode = S_IFREG | shmfd->shm_mode;		/* XXX */
	sb->st_uid = shmfd->shm_uid;
	sb->st_gid = shmfd->shm_gid;
	mtx_unlock(&shm_timestamp_lock);
	sb->st_dev = shm_dev_ino;
	sb->st_ino = shmfd->shm_ino;
	sb->st_nlink = shmfd->shm_object->ref_count;
	sb->st_blocks = shmfd->shm_object->size /
	    (pagesizes[shmfd->shm_lp_psind] >> PAGE_SHIFT);

	return (0);
}

static int
shm_close(struct file *fp, struct thread *td)
{
	struct shmfd *shmfd;

	shmfd = fp->f_data;
	fp->f_data = NULL;
	shm_drop(shmfd);

	return (0);
}

static int
shm_copyin_path(struct thread *td, const char *userpath_in, char **path_out) {
	int error;
	char *path;
	const char *pr_path;
	size_t pr_pathlen;

	path = malloc(MAXPATHLEN, M_SHMFD, M_WAITOK);
	pr_path = td->td_ucred->cr_prison->pr_path;

	/* Construct a full pathname for jailed callers. */
	pr_pathlen = strcmp(pr_path, "/") ==
	    0 ? 0 : strlcpy(path, pr_path, MAXPATHLEN);
	error = copyinstr(userpath_in, path + pr_pathlen,
	    MAXPATHLEN - pr_pathlen, NULL);
	if (error != 0)
		goto out;

#ifdef KTRACE
	if (KTRPOINT(curthread, KTR_NAMEI))
		ktrnamei(path);
#endif

	/* Require paths to start with a '/' character. */
	if (path[pr_pathlen] != '/') {
		error = EINVAL;
		goto out;
	}

	*path_out = path;

out:
	if (error != 0)
		free(path, M_SHMFD);

	return (error);
}

static int
shm_partial_page_invalidate(vm_object_t object, vm_pindex_t idx, int base,
    int end)
{
	vm_page_t m;
	int rv;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(base >= 0, ("%s: base %d", __func__, base));
	KASSERT(end - base <= PAGE_SIZE, ("%s: base %d end %d", __func__, base,
	    end));

retry:
	m = vm_page_grab(object, idx, VM_ALLOC_NOCREAT);
	if (m != NULL) {
		MPASS(vm_page_all_valid(m));
	} else if (vm_pager_has_page(object, idx, NULL, NULL)) {
		m = vm_page_alloc(object, idx,
		    VM_ALLOC_NORMAL | VM_ALLOC_WAITFAIL);
		if (m == NULL)
			goto retry;
		vm_object_pip_add(object, 1);
		VM_OBJECT_WUNLOCK(object);
		rv = vm_pager_get_pages(object, &m, 1, NULL, NULL);
		VM_OBJECT_WLOCK(object);
		vm_object_pip_wakeup(object);
		if (rv == VM_PAGER_OK) {
			/*
			 * Since the page was not resident, and therefore not
			 * recently accessed, immediately enqueue it for
			 * asynchronous laundering.  The current operation is
			 * not regarded as an access.
			 */
			vm_page_launder(m);
		} else {
			vm_page_free(m);
			VM_OBJECT_WUNLOCK(object);
			return (EIO);
		}
	}
	if (m != NULL) {
		pmap_zero_page_area(m, base, end - base);
		KASSERT(vm_page_all_valid(m), ("%s: page %p is invalid",
		    __func__, m));
		vm_page_set_dirty(m);
		vm_page_xunbusy(m);
	}

	return (0);
}

static int
shm_dotruncate_locked(struct shmfd *shmfd, off_t length, void *rl_cookie)
{
	vm_object_t object;
	vm_pindex_t nobjsize;
	vm_ooffset_t delta;
	int base, error;

	KASSERT(length >= 0, ("shm_dotruncate: length < 0"));
	object = shmfd->shm_object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	rangelock_cookie_assert(rl_cookie, RA_WLOCKED);
	if (length == shmfd->shm_size)
		return (0);
	nobjsize = OFF_TO_IDX(length + PAGE_MASK);

	/* Are we shrinking?  If so, trim the end. */
	if (length < shmfd->shm_size) {
		if ((shmfd->shm_seals & F_SEAL_SHRINK) != 0)
			return (EPERM);

		/*
		 * Disallow any requests to shrink the size if this
		 * object is mapped into the kernel.
		 */
		if (shmfd->shm_kmappings > 0)
			return (EBUSY);

		/*
		 * Zero the truncated part of the last page.
		 */
		base = length & PAGE_MASK;
		if (base != 0) {
			error = shm_partial_page_invalidate(object,
			    OFF_TO_IDX(length), base, PAGE_SIZE);
			if (error)
				return (error);
		}
		delta = IDX_TO_OFF(object->size - nobjsize);

		if (nobjsize < object->size)
			vm_object_page_remove(object, nobjsize, object->size,
			    0);

		/* Free the swap accounted for shm */
		swap_release_by_cred(delta, object->cred);
		object->charge -= delta;
	} else {
		if ((shmfd->shm_seals & F_SEAL_GROW) != 0)
			return (EPERM);

		/* Try to reserve additional swap space. */
		delta = IDX_TO_OFF(nobjsize - object->size);
		if (!swap_reserve_by_cred(delta, object->cred))
			return (ENOMEM);
		object->charge += delta;
	}
	shmfd->shm_size = length;
	mtx_lock(&shm_timestamp_lock);
	vfs_timestamp(&shmfd->shm_ctime);
	shmfd->shm_mtime = shmfd->shm_ctime;
	mtx_unlock(&shm_timestamp_lock);
	object->size = nobjsize;
	return (0);
}

static int
shm_dotruncate_largepage(struct shmfd *shmfd, off_t length, void *rl_cookie)
{
	vm_object_t object;
	vm_page_t m;
	vm_pindex_t newobjsz;
	vm_pindex_t oldobjsz __unused;
	int aflags, error, i, psind, try;

	KASSERT(length >= 0, ("shm_dotruncate: length < 0"));
	object = shmfd->shm_object;
	VM_OBJECT_ASSERT_WLOCKED(object);
	rangelock_cookie_assert(rl_cookie, RA_WLOCKED);

	oldobjsz = object->size;
	newobjsz = OFF_TO_IDX(length);
	if (length == shmfd->shm_size)
		return (0);
	psind = shmfd->shm_lp_psind;
	if (psind == 0 && length != 0)
		return (EINVAL);
	if ((length & (pagesizes[psind] - 1)) != 0)
		return (EINVAL);

	if (length < shmfd->shm_size) {
		if ((shmfd->shm_seals & F_SEAL_SHRINK) != 0)
			return (EPERM);
		if (shmfd->shm_kmappings > 0)
			return (EBUSY);
		return (ENOTSUP);	/* Pages are unmanaged. */
#if 0
		vm_object_page_remove(object, newobjsz, oldobjsz, 0);
		object->size = newobjsz;
		shmfd->shm_size = length;
		return (0);
#endif
	}

	if ((shmfd->shm_seals & F_SEAL_GROW) != 0)
		return (EPERM);

	aflags = VM_ALLOC_NORMAL | VM_ALLOC_ZERO;
	if (shmfd->shm_lp_alloc_policy == SHM_LARGEPAGE_ALLOC_NOWAIT)
		aflags |= VM_ALLOC_WAITFAIL;
	try = 0;

	/*
	 * Extend shmfd and object, keeping all already fully
	 * allocated large pages intact even on error, because dropped
	 * object lock might allowed mapping of them.
	 */
	while (object->size < newobjsz) {
		m = vm_page_alloc_contig(object, object->size, aflags,
		    pagesizes[psind] / PAGE_SIZE, 0, ~0,
		    pagesizes[psind], 0,
		    VM_MEMATTR_DEFAULT);
		if (m == NULL) {
			VM_OBJECT_WUNLOCK(object);
			if (shmfd->shm_lp_alloc_policy ==
			    SHM_LARGEPAGE_ALLOC_NOWAIT ||
			    (shmfd->shm_lp_alloc_policy ==
			    SHM_LARGEPAGE_ALLOC_DEFAULT &&
			    try >= largepage_reclaim_tries)) {
				VM_OBJECT_WLOCK(object);
				return (ENOMEM);
			}
			error = vm_page_reclaim_contig(aflags,
			    pagesizes[psind] / PAGE_SIZE, 0, ~0,
			    pagesizes[psind], 0) ? 0 :
			    vm_wait_intr(object);
			if (error != 0) {
				VM_OBJECT_WLOCK(object);
				return (error);
			}
			try++;
			VM_OBJECT_WLOCK(object);
			continue;
		}
		try = 0;
		for (i = 0; i < pagesizes[psind] / PAGE_SIZE; i++) {
			if ((m[i].flags & PG_ZERO) == 0)
				pmap_zero_page(&m[i]);
			vm_page_valid(&m[i]);
			vm_page_xunbusy(&m[i]);
		}
		object->size += OFF_TO_IDX(pagesizes[psind]);
		shmfd->shm_size += pagesizes[psind];
		atomic_add_long(&count_largepages[psind], 1);
		vm_wire_add(atop(pagesizes[psind]));
	}
	return (0);
}

static int
shm_dotruncate_cookie(struct shmfd *shmfd, off_t length, void *rl_cookie)
{
	int error;

	VM_OBJECT_WLOCK(shmfd->shm_object);
	error = shm_largepage(shmfd) ? shm_dotruncate_largepage(shmfd,
	    length, rl_cookie) : shm_dotruncate_locked(shmfd, length,
	    rl_cookie);
	VM_OBJECT_WUNLOCK(shmfd->shm_object);
	return (error);
}

int
shm_dotruncate(struct shmfd *shmfd, off_t length)
{
	void *rl_cookie;
	int error;

	rl_cookie = rangelock_wlock(&shmfd->shm_rl, 0, OFF_MAX,
	    &shmfd->shm_mtx);
	error = shm_dotruncate_cookie(shmfd, length, rl_cookie);
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	return (error);
}

/*
 * shmfd object management including creation and reference counting
 * routines.
 */
struct shmfd *
shm_alloc(struct ucred *ucred, mode_t mode, bool largepage)
{
	struct shmfd *shmfd;

	shmfd = malloc(sizeof(*shmfd), M_SHMFD, M_WAITOK | M_ZERO);
	shmfd->shm_size = 0;
	shmfd->shm_uid = ucred->cr_uid;
	shmfd->shm_gid = ucred->cr_gid;
	shmfd->shm_mode = mode;
	if (largepage) {
		shmfd->shm_object = phys_pager_allocate(NULL,
		    &shm_largepage_phys_ops, NULL, shmfd->shm_size,
		    VM_PROT_DEFAULT, 0, ucred);
		shmfd->shm_lp_alloc_policy = SHM_LARGEPAGE_ALLOC_DEFAULT;
	} else {
		shmfd->shm_object = vm_pager_allocate(OBJT_SWAP, NULL,
		    shmfd->shm_size, VM_PROT_DEFAULT, 0, ucred);
	}
	KASSERT(shmfd->shm_object != NULL, ("shm_create: vm_pager_allocate"));
	vfs_timestamp(&shmfd->shm_birthtime);
	shmfd->shm_atime = shmfd->shm_mtime = shmfd->shm_ctime =
	    shmfd->shm_birthtime;
	shmfd->shm_ino = alloc_unr64(&shm_ino_unr);
	refcount_init(&shmfd->shm_refs, 1);
	mtx_init(&shmfd->shm_mtx, "shmrl", NULL, MTX_DEF);
	rangelock_init(&shmfd->shm_rl);
#ifdef MAC
	mac_posixshm_init(shmfd);
	mac_posixshm_create(ucred, shmfd);
#endif

	return (shmfd);
}

struct shmfd *
shm_hold(struct shmfd *shmfd)
{

	refcount_acquire(&shmfd->shm_refs);
	return (shmfd);
}

void
shm_drop(struct shmfd *shmfd)
{

	if (refcount_release(&shmfd->shm_refs)) {
#ifdef MAC
		mac_posixshm_destroy(shmfd);
#endif
		rangelock_destroy(&shmfd->shm_rl);
		mtx_destroy(&shmfd->shm_mtx);
		vm_object_deallocate(shmfd->shm_object);
		free(shmfd, M_SHMFD);
	}
}

/*
 * Determine if the credentials have sufficient permissions for a
 * specified combination of FREAD and FWRITE.
 */
int
shm_access(struct shmfd *shmfd, struct ucred *ucred, int flags)
{
	accmode_t accmode;
	int error;

	accmode = 0;
	if (flags & FREAD)
		accmode |= VREAD;
	if (flags & FWRITE)
		accmode |= VWRITE;
	mtx_lock(&shm_timestamp_lock);
	error = vaccess(VREG, shmfd->shm_mode, shmfd->shm_uid, shmfd->shm_gid,
	    accmode, ucred);
	mtx_unlock(&shm_timestamp_lock);
	return (error);
}

static void
shm_init(void *arg)
{
	char name[32];
	int i;

	mtx_init(&shm_timestamp_lock, "shm timestamps", NULL, MTX_DEF);
	sx_init(&shm_dict_lock, "shm dictionary");
	shm_dictionary = hashinit(1024, M_SHMFD, &shm_hash);
	new_unrhdr64(&shm_ino_unr, 1);
	shm_dev_ino = devfs_alloc_cdp_inode();
	KASSERT(shm_dev_ino > 0, ("shm dev inode not initialized"));

	for (i = 1; i < MAXPAGESIZES; i++) {
		if (pagesizes[i] == 0)
			break;
#define	M	(1024 * 1024)
#define	G	(1024 * M)
		if (pagesizes[i] >= G)
			snprintf(name, sizeof(name), "%luG", pagesizes[i] / G);
		else if (pagesizes[i] >= M)
			snprintf(name, sizeof(name), "%luM", pagesizes[i] / M);
		else
			snprintf(name, sizeof(name), "%lu", pagesizes[i]);
#undef G
#undef M
		SYSCTL_ADD_ULONG(NULL, SYSCTL_STATIC_CHILDREN(_vm_largepages),
		    OID_AUTO, name, CTLFLAG_RD, &count_largepages[i],
		    "number of non-transient largepages allocated");
	}
}
SYSINIT(shm_init, SI_SUB_SYSV_SHM, SI_ORDER_ANY, shm_init, NULL);

/*
 * Dictionary management.  We maintain an in-kernel dictionary to map
 * paths to shmfd objects.  We use the FNV hash on the path to store
 * the mappings in a hash table.
 */
static struct shmfd *
shm_lookup(char *path, Fnv32_t fnv)
{
	struct shm_mapping *map;

	LIST_FOREACH(map, SHM_HASH(fnv), sm_link) {
		if (map->sm_fnv != fnv)
			continue;
		if (strcmp(map->sm_path, path) == 0)
			return (map->sm_shmfd);
	}

	return (NULL);
}

static void
shm_insert(char *path, Fnv32_t fnv, struct shmfd *shmfd)
{
	struct shm_mapping *map;

	map = malloc(sizeof(struct shm_mapping), M_SHMFD, M_WAITOK);
	map->sm_path = path;
	map->sm_fnv = fnv;
	map->sm_shmfd = shm_hold(shmfd);
	shmfd->shm_path = path;
	LIST_INSERT_HEAD(SHM_HASH(fnv), map, sm_link);
}

static int
shm_remove(char *path, Fnv32_t fnv, struct ucred *ucred)
{
	struct shm_mapping *map;
	int error;

	LIST_FOREACH(map, SHM_HASH(fnv), sm_link) {
		if (map->sm_fnv != fnv)
			continue;
		if (strcmp(map->sm_path, path) == 0) {
#ifdef MAC
			error = mac_posixshm_check_unlink(ucred, map->sm_shmfd);
			if (error)
				return (error);
#endif
			error = shm_access(map->sm_shmfd, ucred,
			    FREAD | FWRITE);
			if (error)
				return (error);
			map->sm_shmfd->shm_path = NULL;
			LIST_REMOVE(map, sm_link);
			shm_drop(map->sm_shmfd);
			free(map->sm_path, M_SHMFD);
			free(map, M_SHMFD);
			return (0);
		}
	}

	return (ENOENT);
}

int
kern_shm_open2(struct thread *td, const char *userpath, int flags, mode_t mode,
    int shmflags, struct filecaps *fcaps, const char *name __unused)
{
	struct pwddesc *pdp;
	struct shmfd *shmfd;
	struct file *fp;
	char *path;
	void *rl_cookie;
	Fnv32_t fnv;
	mode_t cmode;
	int error, fd, initial_seals;
	bool largepage;

	if ((shmflags & ~(SHM_ALLOW_SEALING | SHM_GROW_ON_WRITE |
	    SHM_LARGEPAGE)) != 0)
		return (EINVAL);

	initial_seals = F_SEAL_SEAL;
	if ((shmflags & SHM_ALLOW_SEALING) != 0)
		initial_seals &= ~F_SEAL_SEAL;

#ifdef CAPABILITY_MODE
	/*
	 * shm_open(2) is only allowed for anonymous objects.
	 */
	if (IN_CAPABILITY_MODE(td) && (userpath != SHM_ANON))
		return (ECAPMODE);
#endif

	AUDIT_ARG_FFLAGS(flags);
	AUDIT_ARG_MODE(mode);

	if ((flags & O_ACCMODE) != O_RDONLY && (flags & O_ACCMODE) != O_RDWR)
		return (EINVAL);

	if ((flags & ~(O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_CLOEXEC)) != 0)
		return (EINVAL);

	largepage = (shmflags & SHM_LARGEPAGE) != 0;
	if (largepage && !PMAP_HAS_LARGEPAGES)
		return (ENOTTY);

	/*
	 * Currently only F_SEAL_SEAL may be set when creating or opening shmfd.
	 * If the decision is made later to allow additional seals, care must be
	 * taken below to ensure that the seals are properly set if the shmfd
	 * already existed -- this currently assumes that only F_SEAL_SEAL can
	 * be set and doesn't take further precautions to ensure the validity of
	 * the seals being added with respect to current mappings.
	 */
	if ((initial_seals & ~F_SEAL_SEAL) != 0)
		return (EINVAL);

	pdp = td->td_proc->p_pd;
	cmode = (mode & ~pdp->pd_cmask) & ACCESSPERMS;

	/*
	 * shm_open(2) created shm should always have O_CLOEXEC set, as mandated
	 * by POSIX.  We allow it to be unset here so that an in-kernel
	 * interface may be written as a thin layer around shm, optionally not
	 * setting CLOEXEC.  For shm_open(2), O_CLOEXEC is set unconditionally
	 * in sys_shm_open() to keep this implementation compliant.
	 */
	error = falloc_caps(td, &fp, &fd, flags & O_CLOEXEC, fcaps);
	if (error)
		return (error);

	/* A SHM_ANON path pointer creates an anonymous object. */
	if (userpath == SHM_ANON) {
		/* A read-only anonymous object is pointless. */
		if ((flags & O_ACCMODE) == O_RDONLY) {
			fdclose(td, fp, fd);
			fdrop(fp, td);
			return (EINVAL);
		}
		shmfd = shm_alloc(td->td_ucred, cmode, largepage);
		shmfd->shm_seals = initial_seals;
		shmfd->shm_flags = shmflags;
	} else {
		error = shm_copyin_path(td, userpath, &path);
		if (error != 0) {
			fdclose(td, fp, fd);
			fdrop(fp, td);
			return (error);
		}

		AUDIT_ARG_UPATH1_CANON(path);
		fnv = fnv_32_str(path, FNV1_32_INIT);
		sx_xlock(&shm_dict_lock);
		shmfd = shm_lookup(path, fnv);
		if (shmfd == NULL) {
			/* Object does not yet exist, create it if requested. */
			if (flags & O_CREAT) {
#ifdef MAC
				error = mac_posixshm_check_create(td->td_ucred,
				    path);
				if (error == 0) {
#endif
					shmfd = shm_alloc(td->td_ucred, cmode,
					    largepage);
					shmfd->shm_seals = initial_seals;
					shmfd->shm_flags = shmflags;
					shm_insert(path, fnv, shmfd);
#ifdef MAC
				}
#endif
			} else {
				free(path, M_SHMFD);
				error = ENOENT;
			}
		} else {
			rl_cookie = rangelock_wlock(&shmfd->shm_rl, 0, OFF_MAX,
			    &shmfd->shm_mtx);

			/*
			 * kern_shm_open() likely shouldn't ever error out on
			 * trying to set a seal that already exists, unlike
			 * F_ADD_SEALS.  This would break terribly as
			 * shm_open(2) actually sets F_SEAL_SEAL to maintain
			 * historical behavior where the underlying file could
			 * not be sealed.
			 */
			initial_seals &= ~shmfd->shm_seals;

			/*
			 * Object already exists, obtain a new
			 * reference if requested and permitted.
			 */
			free(path, M_SHMFD);

			/*
			 * initial_seals can't set additional seals if we've
			 * already been set F_SEAL_SEAL.  If F_SEAL_SEAL is set,
			 * then we've already removed that one from
			 * initial_seals.  This is currently redundant as we
			 * only allow setting F_SEAL_SEAL at creation time, but
			 * it's cheap to check and decreases the effort required
			 * to allow additional seals.
			 */
			if ((shmfd->shm_seals & F_SEAL_SEAL) != 0 &&
			    initial_seals != 0)
				error = EPERM;
			else if ((flags & (O_CREAT | O_EXCL)) ==
			    (O_CREAT | O_EXCL))
				error = EEXIST;
			else if (shmflags != 0 && shmflags != shmfd->shm_flags)
				error = EINVAL;
			else {
#ifdef MAC
				error = mac_posixshm_check_open(td->td_ucred,
				    shmfd, FFLAGS(flags & O_ACCMODE));
				if (error == 0)
#endif
				error = shm_access(shmfd, td->td_ucred,
				    FFLAGS(flags & O_ACCMODE));
			}

			/*
			 * Truncate the file back to zero length if
			 * O_TRUNC was specified and the object was
			 * opened with read/write.
			 */
			if (error == 0 &&
			    (flags & (O_ACCMODE | O_TRUNC)) ==
			    (O_RDWR | O_TRUNC)) {
				VM_OBJECT_WLOCK(shmfd->shm_object);
#ifdef MAC
				error = mac_posixshm_check_truncate(
					td->td_ucred, fp->f_cred, shmfd);
				if (error == 0)
#endif
					error = shm_dotruncate_locked(shmfd, 0,
					    rl_cookie);
				VM_OBJECT_WUNLOCK(shmfd->shm_object);
			}
			if (error == 0) {
				/*
				 * Currently we only allow F_SEAL_SEAL to be
				 * set initially.  As noted above, this would
				 * need to be reworked should that change.
				 */
				shmfd->shm_seals |= initial_seals;
				shm_hold(shmfd);
			}
			rangelock_unlock(&shmfd->shm_rl, rl_cookie,
			    &shmfd->shm_mtx);
		}
		sx_xunlock(&shm_dict_lock);

		if (error) {
			fdclose(td, fp, fd);
			fdrop(fp, td);
			return (error);
		}
	}

	finit(fp, FFLAGS(flags & O_ACCMODE), DTYPE_SHM, shmfd, &shm_ops);

	td->td_retval[0] = fd;
	fdrop(fp, td);

	return (0);
}

/* System calls. */
#ifdef COMPAT_FREEBSD12
int
freebsd12_shm_open(struct thread *td, struct freebsd12_shm_open_args *uap)
{

	return (kern_shm_open(td, uap->path, uap->flags | O_CLOEXEC,
	    uap->mode, NULL));
}
#endif

int
sys_shm_unlink(struct thread *td, struct shm_unlink_args *uap)
{
	char *path;
	Fnv32_t fnv;
	int error;

	error = shm_copyin_path(td, uap->path, &path);
	if (error != 0)
		return (error);

	AUDIT_ARG_UPATH1_CANON(path);
	fnv = fnv_32_str(path, FNV1_32_INIT);
	sx_xlock(&shm_dict_lock);
	error = shm_remove(path, fnv, td->td_ucred);
	sx_xunlock(&shm_dict_lock);
	free(path, M_SHMFD);

	return (error);
}

int
sys_shm_rename(struct thread *td, struct shm_rename_args *uap)
{
	char *path_from = NULL, *path_to = NULL;
	Fnv32_t fnv_from, fnv_to;
	struct shmfd *fd_from;
	struct shmfd *fd_to;
	int error;
	int flags;

	flags = uap->flags;
	AUDIT_ARG_FFLAGS(flags);

	/*
	 * Make sure the user passed only valid flags.
	 * If you add a new flag, please add a new term here.
	 */
	if ((flags & ~(
	    SHM_RENAME_NOREPLACE |
	    SHM_RENAME_EXCHANGE
	    )) != 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * EXCHANGE and NOREPLACE don't quite make sense together. Let's
	 * force the user to choose one or the other.
	 */
	if ((flags & SHM_RENAME_NOREPLACE) != 0 &&
	    (flags & SHM_RENAME_EXCHANGE) != 0) {
		error = EINVAL;
		goto out;
	}

	/* Renaming to or from anonymous makes no sense */
	if (uap->path_from == SHM_ANON || uap->path_to == SHM_ANON) {
		error = EINVAL;
		goto out;
	}

	error = shm_copyin_path(td, uap->path_from, &path_from);
	if (error != 0)
		goto out;

	error = shm_copyin_path(td, uap->path_to, &path_to);
	if (error != 0)
		goto out;

	AUDIT_ARG_UPATH1_CANON(path_from);
	AUDIT_ARG_UPATH2_CANON(path_to);

	/* Rename with from/to equal is a no-op */
	if (strcmp(path_from, path_to) == 0)
		goto out;

	fnv_from = fnv_32_str(path_from, FNV1_32_INIT);
	fnv_to = fnv_32_str(path_to, FNV1_32_INIT);

	sx_xlock(&shm_dict_lock);

	fd_from = shm_lookup(path_from, fnv_from);
	if (fd_from == NULL) {
		error = ENOENT;
		goto out_locked;
	}

	fd_to = shm_lookup(path_to, fnv_to);
	if ((flags & SHM_RENAME_NOREPLACE) != 0 && fd_to != NULL) {
		error = EEXIST;
		goto out_locked;
	}

	/*
	 * Unconditionally prevents shm_remove from invalidating the 'from'
	 * shm's state.
	 */
	shm_hold(fd_from);
	error = shm_remove(path_from, fnv_from, td->td_ucred);

	/*
	 * One of my assumptions failed if ENOENT (e.g. locking didn't
	 * protect us)
	 */
	KASSERT(error != ENOENT, ("Our shm disappeared during shm_rename: %s",
	    path_from));
	if (error != 0) {
		shm_drop(fd_from);
		goto out_locked;
	}

	/*
	 * If we are exchanging, we need to ensure the shm_remove below
	 * doesn't invalidate the dest shm's state.
	 */
	if ((flags & SHM_RENAME_EXCHANGE) != 0 && fd_to != NULL)
		shm_hold(fd_to);

	/*
	 * NOTE: if path_to is not already in the hash, c'est la vie;
	 * it simply means we have nothing already at path_to to unlink.
	 * That is the ENOENT case.
	 *
	 * If we somehow don't have access to unlink this guy, but
	 * did for the shm at path_from, then relink the shm to path_from
	 * and abort with EACCES.
	 *
	 * All other errors: that is weird; let's relink and abort the
	 * operation.
	 */
	error = shm_remove(path_to, fnv_to, td->td_ucred);
	if (error != 0 && error != ENOENT) {
		shm_insert(path_from, fnv_from, fd_from);
		shm_drop(fd_from);
		/* Don't free path_from now, since the hash references it */
		path_from = NULL;
		goto out_locked;
	}

	error = 0;

	shm_insert(path_to, fnv_to, fd_from);

	/* Don't free path_to now, since the hash references it */
	path_to = NULL;

	/* We kept a ref when we removed, and incremented again in insert */
	shm_drop(fd_from);
	KASSERT(fd_from->shm_refs > 0, ("Expected >0 refs; got: %d\n",
	    fd_from->shm_refs));

	if ((flags & SHM_RENAME_EXCHANGE) != 0 && fd_to != NULL) {
		shm_insert(path_from, fnv_from, fd_to);
		path_from = NULL;
		shm_drop(fd_to);
		KASSERT(fd_to->shm_refs > 0, ("Expected >0 refs; got: %d\n",
		    fd_to->shm_refs));
	}

out_locked:
	sx_xunlock(&shm_dict_lock);

out:
	free(path_from, M_SHMFD);
	free(path_to, M_SHMFD);
	return (error);
}

static int
shm_mmap_large(struct shmfd *shmfd, vm_map_t map, vm_offset_t *addr,
    vm_size_t size, vm_prot_t prot, vm_prot_t max_prot, int flags,
    vm_ooffset_t foff, struct thread *td)
{
	struct vmspace *vms;
	vm_map_entry_t next_entry, prev_entry;
	vm_offset_t align, mask, maxaddr;
	int docow, error, rv, try;
	bool curmap;

	if (shmfd->shm_lp_psind == 0)
		return (EINVAL);

	/* MAP_PRIVATE is disabled */
	if ((flags & ~(MAP_SHARED | MAP_FIXED | MAP_EXCL |
	    MAP_NOCORE |
#ifdef MAP_32BIT
	    MAP_32BIT |
#endif
	    MAP_ALIGNMENT_MASK)) != 0)
		return (EINVAL);

	vms = td->td_proc->p_vmspace;
	curmap = map == &vms->vm_map;
	if (curmap) {
		error = kern_mmap_racct_check(td, map, size);
		if (error != 0)
			return (error);
	}

	docow = shmfd->shm_lp_psind << MAP_SPLIT_BOUNDARY_SHIFT;
	docow |= MAP_INHERIT_SHARE;
	if ((flags & MAP_NOCORE) != 0)
		docow |= MAP_DISABLE_COREDUMP;

	mask = pagesizes[shmfd->shm_lp_psind] - 1;
	if ((foff & mask) != 0)
		return (EINVAL);
	maxaddr = vm_map_max(map);
#ifdef MAP_32BIT
	if ((flags & MAP_32BIT) != 0 && maxaddr > MAP_32BIT_MAX_ADDR)
		maxaddr = MAP_32BIT_MAX_ADDR;
#endif
	if (size == 0 || (size & mask) != 0 ||
	    (*addr != 0 && ((*addr & mask) != 0 ||
	    *addr + size < *addr || *addr + size > maxaddr)))
		return (EINVAL);

	align = flags & MAP_ALIGNMENT_MASK;
	if (align == 0) {
		align = pagesizes[shmfd->shm_lp_psind];
	} else if (align == MAP_ALIGNED_SUPER) {
		if (shmfd->shm_lp_psind != 1)
			return (EINVAL);
		align = pagesizes[1];
	} else {
		align >>= MAP_ALIGNMENT_SHIFT;
		align = 1ULL << align;
		/* Also handles overflow. */
		if (align < pagesizes[shmfd->shm_lp_psind])
			return (EINVAL);
	}

	vm_map_lock(map);
	if ((flags & MAP_FIXED) == 0) {
		try = 1;
		if (curmap && (*addr == 0 ||
		    (*addr >= round_page((vm_offset_t)vms->vm_taddr) &&
		    *addr < round_page((vm_offset_t)vms->vm_daddr +
		    lim_max(td, RLIMIT_DATA))))) {
			*addr = roundup2((vm_offset_t)vms->vm_daddr +
			    lim_max(td, RLIMIT_DATA),
			    pagesizes[shmfd->shm_lp_psind]);
		}
again:
		rv = vm_map_find_aligned(map, addr, size, maxaddr, align);
		if (rv != KERN_SUCCESS) {
			if (try == 1) {
				try = 2;
				*addr = vm_map_min(map);
				if ((*addr & mask) != 0)
					*addr = (*addr + mask) & mask;
				goto again;
			}
			goto fail1;
		}
	} else if ((flags & MAP_EXCL) == 0) {
		rv = vm_map_delete(map, *addr, *addr + size);
		if (rv != KERN_SUCCESS)
			goto fail1;
	} else {
		error = ENOSPC;
		if (vm_map_lookup_entry(map, *addr, &prev_entry))
			goto fail;
		next_entry = vm_map_entry_succ(prev_entry);
		if (next_entry->start < *addr + size)
			goto fail;
	}

	rv = vm_map_insert(map, shmfd->shm_object, foff, *addr, *addr + size,
	    prot, max_prot, docow);
fail1:
	error = vm_mmap_to_errno(rv);
fail:
	vm_map_unlock(map);
	return (error);
}

static int
shm_mmap(struct file *fp, vm_map_t map, vm_offset_t *addr, vm_size_t objsize,
    vm_prot_t prot, vm_prot_t cap_maxprot, int flags,
    vm_ooffset_t foff, struct thread *td)
{
	struct shmfd *shmfd;
	vm_prot_t maxprot;
	int error;
	bool writecnt;
	void *rl_cookie;

	shmfd = fp->f_data;
	maxprot = VM_PROT_NONE;

	rl_cookie = rangelock_rlock(&shmfd->shm_rl, 0, objsize,
	    &shmfd->shm_mtx);
	/* FREAD should always be set. */
	if ((fp->f_flag & FREAD) != 0)
		maxprot |= VM_PROT_EXECUTE | VM_PROT_READ;

	/*
	 * If FWRITE's set, we can allow VM_PROT_WRITE unless it's a shared
	 * mapping with a write seal applied.  Private mappings are always
	 * writeable.
	 */
	if ((flags & MAP_SHARED) == 0) {
		cap_maxprot |= VM_PROT_WRITE;
		maxprot |= VM_PROT_WRITE;
		writecnt = false;
	} else {
		if ((fp->f_flag & FWRITE) != 0 &&
		    (shmfd->shm_seals & F_SEAL_WRITE) == 0)
			maxprot |= VM_PROT_WRITE;

		/*
		 * Any mappings from a writable descriptor may be upgraded to
		 * VM_PROT_WRITE with mprotect(2), unless a write-seal was
		 * applied between the open and subsequent mmap(2).  We want to
		 * reject application of a write seal as long as any such
		 * mapping exists so that the seal cannot be trivially bypassed.
		 */
		writecnt = (maxprot & VM_PROT_WRITE) != 0;
		if (!writecnt && (prot & VM_PROT_WRITE) != 0) {
			error = EACCES;
			goto out;
		}
	}
	maxprot &= cap_maxprot;

	/* See comment in vn_mmap(). */
	if (
#ifdef _LP64
	    objsize > OFF_MAX ||
#endif
	    foff > OFF_MAX - objsize) {
		error = EINVAL;
		goto out;
	}

#ifdef MAC
	error = mac_posixshm_check_mmap(td->td_ucred, shmfd, prot, flags);
	if (error != 0)
		goto out;
#endif

	mtx_lock(&shm_timestamp_lock);
	vfs_timestamp(&shmfd->shm_atime);
	mtx_unlock(&shm_timestamp_lock);
	vm_object_reference(shmfd->shm_object);

	if (shm_largepage(shmfd)) {
		writecnt = false;
		error = shm_mmap_large(shmfd, map, addr, objsize, prot,
		    maxprot, flags, foff, td);
	} else {
		if (writecnt) {
			vm_pager_update_writecount(shmfd->shm_object, 0,
			    objsize);
		}
		error = vm_mmap_object(map, addr, objsize, prot, maxprot, flags,
		    shmfd->shm_object, foff, writecnt, td);
	}
	if (error != 0) {
		if (writecnt)
			vm_pager_release_writecount(shmfd->shm_object, 0,
			    objsize);
		vm_object_deallocate(shmfd->shm_object);
	}
out:
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	return (error);
}

static int
shm_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{
	struct shmfd *shmfd;
	int error;

	error = 0;
	shmfd = fp->f_data;
	mtx_lock(&shm_timestamp_lock);
	/*
	 * SUSv4 says that x bits of permission need not be affected.
	 * Be consistent with our shm_open there.
	 */
#ifdef MAC
	error = mac_posixshm_check_setmode(active_cred, shmfd, mode);
	if (error != 0)
		goto out;
#endif
	error = vaccess(VREG, shmfd->shm_mode, shmfd->shm_uid, shmfd->shm_gid,
	    VADMIN, active_cred);
	if (error != 0)
		goto out;
	shmfd->shm_mode = mode & ACCESSPERMS;
out:
	mtx_unlock(&shm_timestamp_lock);
	return (error);
}

static int
shm_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{
	struct shmfd *shmfd;
	int error;

	error = 0;
	shmfd = fp->f_data;
	mtx_lock(&shm_timestamp_lock);
#ifdef MAC
	error = mac_posixshm_check_setowner(active_cred, shmfd, uid, gid);
	if (error != 0)
		goto out;
#endif
	if (uid == (uid_t)-1)
		uid = shmfd->shm_uid;
	if (gid == (gid_t)-1)
                 gid = shmfd->shm_gid;
	if (((uid != shmfd->shm_uid && uid != active_cred->cr_uid) ||
	    (gid != shmfd->shm_gid && !groupmember(gid, active_cred))) &&
	    (error = priv_check_cred(active_cred, PRIV_VFS_CHOWN)))
		goto out;
	shmfd->shm_uid = uid;
	shmfd->shm_gid = gid;
out:
	mtx_unlock(&shm_timestamp_lock);
	return (error);
}

/*
 * Helper routines to allow the backing object of a shared memory file
 * descriptor to be mapped in the kernel.
 */
int
shm_map(struct file *fp, size_t size, off_t offset, void **memp)
{
	struct shmfd *shmfd;
	vm_offset_t kva, ofs;
	vm_object_t obj;
	int rv;

	if (fp->f_type != DTYPE_SHM)
		return (EINVAL);
	shmfd = fp->f_data;
	obj = shmfd->shm_object;
	VM_OBJECT_WLOCK(obj);
	/*
	 * XXXRW: This validation is probably insufficient, and subject to
	 * sign errors.  It should be fixed.
	 */
	if (offset >= shmfd->shm_size ||
	    offset + size > round_page(shmfd->shm_size)) {
		VM_OBJECT_WUNLOCK(obj);
		return (EINVAL);
	}

	shmfd->shm_kmappings++;
	vm_object_reference_locked(obj);
	VM_OBJECT_WUNLOCK(obj);

	/* Map the object into the kernel_map and wire it. */
	kva = vm_map_min(kernel_map);
	ofs = offset & PAGE_MASK;
	offset = trunc_page(offset);
	size = round_page(size + ofs);
	rv = vm_map_find(kernel_map, obj, offset, &kva, size, 0,
	    VMFS_OPTIMAL_SPACE, VM_PROT_READ | VM_PROT_WRITE,
	    VM_PROT_READ | VM_PROT_WRITE, 0);
	if (rv == KERN_SUCCESS) {
		rv = vm_map_wire(kernel_map, kva, kva + size,
		    VM_MAP_WIRE_SYSTEM | VM_MAP_WIRE_NOHOLES);
		if (rv == KERN_SUCCESS) {
			*memp = (void *)(kva + ofs);
			return (0);
		}
		vm_map_remove(kernel_map, kva, kva + size);
	} else
		vm_object_deallocate(obj);

	/* On failure, drop our mapping reference. */
	VM_OBJECT_WLOCK(obj);
	shmfd->shm_kmappings--;
	VM_OBJECT_WUNLOCK(obj);

	return (vm_mmap_to_errno(rv));
}

/*
 * We require the caller to unmap the entire entry.  This allows us to
 * safely decrement shm_kmappings when a mapping is removed.
 */
int
shm_unmap(struct file *fp, void *mem, size_t size)
{
	struct shmfd *shmfd;
	vm_map_entry_t entry;
	vm_offset_t kva, ofs;
	vm_object_t obj;
	vm_pindex_t pindex;
	vm_prot_t prot;
	boolean_t wired;
	vm_map_t map;
	int rv;

	if (fp->f_type != DTYPE_SHM)
		return (EINVAL);
	shmfd = fp->f_data;
	kva = (vm_offset_t)mem;
	ofs = kva & PAGE_MASK;
	kva = trunc_page(kva);
	size = round_page(size + ofs);
	map = kernel_map;
	rv = vm_map_lookup(&map, kva, VM_PROT_READ | VM_PROT_WRITE, &entry,
	    &obj, &pindex, &prot, &wired);
	if (rv != KERN_SUCCESS)
		return (EINVAL);
	if (entry->start != kva || entry->end != kva + size) {
		vm_map_lookup_done(map, entry);
		return (EINVAL);
	}
	vm_map_lookup_done(map, entry);
	if (obj != shmfd->shm_object)
		return (EINVAL);
	vm_map_remove(map, kva, kva + size);
	VM_OBJECT_WLOCK(obj);
	KASSERT(shmfd->shm_kmappings > 0, ("shm_unmap: object not mapped"));
	shmfd->shm_kmappings--;
	VM_OBJECT_WUNLOCK(obj);
	return (0);
}

static int
shm_fill_kinfo_locked(struct shmfd *shmfd, struct kinfo_file *kif, bool list)
{
	const char *path, *pr_path;
	size_t pr_pathlen;
	bool visible;

	sx_assert(&shm_dict_lock, SA_LOCKED);
	kif->kf_type = KF_TYPE_SHM;
	kif->kf_un.kf_file.kf_file_mode = S_IFREG | shmfd->shm_mode;
	kif->kf_un.kf_file.kf_file_size = shmfd->shm_size;
	if (shmfd->shm_path != NULL) {
		if (shmfd->shm_path != NULL) {
			path = shmfd->shm_path;
			pr_path = curthread->td_ucred->cr_prison->pr_path;
			if (strcmp(pr_path, "/") != 0) {
				/* Return the jail-rooted pathname. */
				pr_pathlen = strlen(pr_path);
				visible = strncmp(path, pr_path, pr_pathlen)
				    == 0 && path[pr_pathlen] == '/';
				if (list && !visible)
					return (EPERM);
				if (visible)
					path += pr_pathlen;
			}
			strlcpy(kif->kf_path, path, sizeof(kif->kf_path));
		}
	}
	return (0);
}

static int
shm_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp __unused)
{
	int res;

	sx_slock(&shm_dict_lock);
	res = shm_fill_kinfo_locked(fp->f_data, kif, false);
	sx_sunlock(&shm_dict_lock);
	return (res);
}

static int
shm_add_seals(struct file *fp, int seals)
{
	struct shmfd *shmfd;
	void *rl_cookie;
	vm_ooffset_t writemappings;
	int error, nseals;

	error = 0;
	shmfd = fp->f_data;
	rl_cookie = rangelock_wlock(&shmfd->shm_rl, 0, OFF_MAX,
	    &shmfd->shm_mtx);

	/* Even already-set seals should result in EPERM. */
	if ((shmfd->shm_seals & F_SEAL_SEAL) != 0) {
		error = EPERM;
		goto out;
	}
	nseals = seals & ~shmfd->shm_seals;
	if ((nseals & F_SEAL_WRITE) != 0) {
		if (shm_largepage(shmfd)) {
			error = ENOTSUP;
			goto out;
		}

		/*
		 * The rangelock above prevents writable mappings from being
		 * added after we've started applying seals.  The RLOCK here
		 * is to avoid torn reads on ILP32 arches as unmapping/reducing
		 * writemappings will be done without a rangelock.
		 */
		VM_OBJECT_RLOCK(shmfd->shm_object);
		writemappings = shmfd->shm_object->un_pager.swp.writemappings;
		VM_OBJECT_RUNLOCK(shmfd->shm_object);
		/* kmappings are also writable */
		if (writemappings > 0) {
			error = EBUSY;
			goto out;
		}
	}
	shmfd->shm_seals |= nseals;
out:
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	return (error);
}

static int
shm_get_seals(struct file *fp, int *seals)
{
	struct shmfd *shmfd;

	shmfd = fp->f_data;
	*seals = shmfd->shm_seals;
	return (0);
}

static int
shm_deallocate(struct shmfd *shmfd, off_t *offset, off_t *length, int flags)
{
	vm_object_t object;
	vm_pindex_t pistart, pi, piend;
	vm_ooffset_t off, len;
	int startofs, endofs, end;
	int error;

	off = *offset;
	len = *length;
	KASSERT(off + len <= (vm_ooffset_t)OFF_MAX, ("off + len overflows"));
	if (off + len > shmfd->shm_size)
		len = shmfd->shm_size - off;
	object = shmfd->shm_object;
	startofs = off & PAGE_MASK;
	endofs = (off + len) & PAGE_MASK;
	pistart = OFF_TO_IDX(off);
	piend = OFF_TO_IDX(off + len);
	pi = OFF_TO_IDX(off + PAGE_MASK);
	error = 0;

	/* Handle the case when offset is on or beyond shm size. */
	if ((off_t)len <= 0) {
		*length = 0;
		return (0);
	}

	VM_OBJECT_WLOCK(object);

	if (startofs != 0) {
		end = pistart != piend ? PAGE_SIZE : endofs;
		error = shm_partial_page_invalidate(object, pistart, startofs,
		    end);
		if (error)
			goto out;
		off += end - startofs;
		len -= end - startofs;
	}

	if (pi < piend) {
		vm_object_page_remove(object, pi, piend, 0);
		off += IDX_TO_OFF(piend - pi);
		len -= IDX_TO_OFF(piend - pi);
	}

	if (endofs != 0 && pistart != piend) {
		error = shm_partial_page_invalidate(object, piend, 0, endofs);
		if (error)
			goto out;
		off += endofs;
		len -= endofs;
	}

out:
	VM_OBJECT_WUNLOCK(shmfd->shm_object);
	*offset = off;
	*length = len;
	return (error);
}

static int
shm_fspacectl(struct file *fp, int cmd, off_t *offset, off_t *length, int flags,
    struct ucred *active_cred, struct thread *td)
{
	void *rl_cookie;
	struct shmfd *shmfd;
	off_t off, len;
	int error;

	/* This assumes that the caller already checked for overflow. */
	error = EINVAL;
	shmfd = fp->f_data;
	off = *offset;
	len = *length;

	if (cmd != SPACECTL_DEALLOC || off < 0 || len <= 0 ||
	    len > OFF_MAX - off || flags != 0)
		return (EINVAL);

	rl_cookie = rangelock_wlock(&shmfd->shm_rl, off, off + len,
	    &shmfd->shm_mtx);
	switch (cmd) {
	case SPACECTL_DEALLOC:
		if ((shmfd->shm_seals & F_SEAL_WRITE) != 0) {
			error = EPERM;
			break;
		}
		error = shm_deallocate(shmfd, &off, &len, flags);
		*offset = off;
		*length = len;
		break;
	default:
		__assert_unreachable();
	}
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	return (error);
}


static int
shm_fallocate(struct file *fp, off_t offset, off_t len, struct thread *td)
{
	void *rl_cookie;
	struct shmfd *shmfd;
	size_t size;
	int error;

	/* This assumes that the caller already checked for overflow. */
	error = 0;
	shmfd = fp->f_data;
	size = offset + len;

	/*
	 * Just grab the rangelock for the range that we may be attempting to
	 * grow, rather than blocking read/write for regions we won't be
	 * touching while this (potential) resize is in progress.  Other
	 * attempts to resize the shmfd will have to take a write lock from 0 to
	 * OFF_MAX, so this being potentially beyond the current usable range of
	 * the shmfd is not necessarily a concern.  If other mechanisms are
	 * added to grow a shmfd, this may need to be re-evaluated.
	 */
	rl_cookie = rangelock_wlock(&shmfd->shm_rl, offset, size,
	    &shmfd->shm_mtx);
	if (size > shmfd->shm_size)
		error = shm_dotruncate_cookie(shmfd, size, rl_cookie);
	rangelock_unlock(&shmfd->shm_rl, rl_cookie, &shmfd->shm_mtx);
	/* Translate to posix_fallocate(2) return value as needed. */
	if (error == ENOMEM)
		error = ENOSPC;
	return (error);
}

static int
sysctl_posix_shm_list(SYSCTL_HANDLER_ARGS)
{
	struct shm_mapping *shmm;
	struct sbuf sb;
	struct kinfo_file kif;
	u_long i;
	ssize_t curlen;
	int error, error2;

	sbuf_new_for_sysctl(&sb, NULL, sizeof(struct kinfo_file) * 5, req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	curlen = 0;
	error = 0;
	sx_slock(&shm_dict_lock);
	for (i = 0; i < shm_hash + 1; i++) {
		LIST_FOREACH(shmm, &shm_dictionary[i], sm_link) {
			error = shm_fill_kinfo_locked(shmm->sm_shmfd,
			    &kif, true);
			if (error == EPERM) {
				error = 0;
				continue;
			}
			if (error != 0)
				break;
			pack_kinfo(&kif);
			error = sbuf_bcat(&sb, &kif, kif.kf_structsize) == 0 ?
			    0 : ENOMEM;
			if (error != 0)
				break;
			curlen += kif.kf_structsize;
		}
	}
	sx_sunlock(&shm_dict_lock);
	error2 = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error != 0 ? error : error2);
}

SYSCTL_PROC(_kern_ipc, OID_AUTO, posix_shm_list,
    CTLFLAG_RD | CTLFLAG_MPSAFE | CTLTYPE_OPAQUE,
    NULL, 0, sysctl_posix_shm_list, "",
    "POSIX SHM list");

int
kern_shm_open(struct thread *td, const char *path, int flags, mode_t mode,
    struct filecaps *caps)
{

	return (kern_shm_open2(td, path, flags, mode, 0, caps, NULL));
}

/*
 * This version of the shm_open() interface leaves CLOEXEC behavior up to the
 * caller, and libc will enforce it for the traditional shm_open() call.  This
 * allows other consumers, like memfd_create(), to opt-in for CLOEXEC.  This
 * interface also includes a 'name' argument that is currently unused, but could
 * potentially be exported later via some interface for debugging purposes.
 * From the kernel's perspective, it is optional.  Individual consumers like
 * memfd_create() may require it in order to be compatible with other systems
 * implementing the same function.
 */
int
sys_shm_open2(struct thread *td, struct shm_open2_args *uap)
{

	return (kern_shm_open2(td, uap->path, uap->flags, uap->mode,
	    uap->shmflags, NULL, uap->name));
}
