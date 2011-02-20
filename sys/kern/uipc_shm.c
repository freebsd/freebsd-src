/*-
 * Copyright (c) 2006 Robert N. M. Watson
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
 */

/*
 * Support for shared swap-backed anonymous memory objects via
 * shm_open(2) and shm_unlink(2).  While most of the implementation is
 * here, vm_mmap.c contains mapping logic changes.
 *
 * TODO:
 *
 * (2) Need to export data to a userland tool via a sysctl.  Should ipcs(1)
 *     and ipcrm(1) be expanded or should new tools to manage both POSIX
 *     kernel semaphores and POSIX shared memory be written?
 *
 * (3) Add support for this file type to fstat(1).
 *
 * (4) Resource limits?  Does this need its own resource limits or are the
 *     existing limits in mmap(2) sufficient?
 *
 * (5) Partial page truncation.  vnode_pager_setsize() will zero any parts
 *     of a partially mapped page as a result of ftruncate(2)/truncate(2).
 *     We can do the same (with the same pmap evil), but do we need to
 *     worry about the bits on disk if the page is swapped out or will the
 *     swapper zero the parts of a page that are invalid if the page is
 *     swapped back in for us?
 *
 * (6) Add MAC support in mac_biba(4) and mac_mls(4).
 *
 * (7) Add a MAC check_create() hook for creating new named objects.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/fnv_hash.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
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

#define	SHM_HASH(fnv)	(&shm_dictionary[(fnv) & shm_hash])

static int	shm_access(struct shmfd *shmfd, struct ucred *ucred, int flags);
static struct shmfd *shm_alloc(struct ucred *ucred, mode_t mode);
static void	shm_dict_init(void *arg);
static void	shm_drop(struct shmfd *shmfd);
static struct shmfd *shm_hold(struct shmfd *shmfd);
static void	shm_insert(char *path, Fnv32_t fnv, struct shmfd *shmfd);
static struct shmfd *shm_lookup(char *path, Fnv32_t fnv);
static int	shm_remove(char *path, Fnv32_t fnv, struct ucred *ucred);
static int	shm_dotruncate(struct shmfd *shmfd, off_t length);

static fo_rdwr_t	shm_read;
static fo_rdwr_t	shm_write;
static fo_truncate_t	shm_truncate;
static fo_ioctl_t	shm_ioctl;
static fo_poll_t	shm_poll;
static fo_kqfilter_t	shm_kqfilter;
static fo_stat_t	shm_stat;
static fo_close_t	shm_close;

/* File descriptor operations. */
static struct fileops shm_ops = {
	.fo_read = shm_read,
	.fo_write = shm_write,
	.fo_truncate = shm_truncate,
	.fo_ioctl = shm_ioctl,
	.fo_poll = shm_poll,
	.fo_kqfilter = shm_kqfilter,
	.fo_stat = shm_stat,
	.fo_close = shm_close,
	.fo_flags = DFLAG_PASSABLE
};

FEATURE(posix_shm, "POSIX shared memory");

static int
shm_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
shm_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EOPNOTSUPP);
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

static int
shm_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
shm_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	return (EOPNOTSUPP);
}

static int
shm_kqfilter(struct file *fp, struct knote *kn)
{

	return (EOPNOTSUPP);
}

static int
shm_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
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
	sb->st_mode = S_IFREG | shmfd->shm_mode;		/* XXX */
	sb->st_blksize = PAGE_SIZE;
	sb->st_size = shmfd->shm_size;
	sb->st_blocks = (sb->st_size + sb->st_blksize - 1) / sb->st_blksize;
	sb->st_atim = shmfd->shm_atime;
	sb->st_ctim = shmfd->shm_ctime;
	sb->st_mtim = shmfd->shm_mtime;
	sb->st_birthtim = shmfd->shm_birthtime;	
	sb->st_uid = shmfd->shm_uid;
	sb->st_gid = shmfd->shm_gid;

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
shm_dotruncate(struct shmfd *shmfd, off_t length)
{
	vm_object_t object;
	vm_page_t m;
	vm_pindex_t nobjsize;
	vm_ooffset_t delta;

	object = shmfd->shm_object;
	VM_OBJECT_LOCK(object);
	if (length == shmfd->shm_size) {
		VM_OBJECT_UNLOCK(object);
		return (0);
	}
	nobjsize = OFF_TO_IDX(length + PAGE_MASK);

	/* Are we shrinking?  If so, trim the end. */
	if (length < shmfd->shm_size) {
		delta = ptoa(object->size - nobjsize);

		/* Toss in memory pages. */
		if (nobjsize < object->size)
			vm_object_page_remove(object, nobjsize, object->size,
			    FALSE);

		/* Toss pages from swap. */
		if (object->type == OBJT_SWAP)
			swap_pager_freespace(object, nobjsize, delta);

		/* Free the swap accounted for shm */
		swap_release_by_cred(delta, object->cred);
		object->charge -= delta;

		/*
		 * If the last page is partially mapped, then zero out
		 * the garbage at the end of the page.  See comments
		 * in vnode_pager_setsize() for more details.
		 *
		 * XXXJHB: This handles in memory pages, but what about
		 * a page swapped out to disk?
		 */
		if ((length & PAGE_MASK) &&
		    (m = vm_page_lookup(object, OFF_TO_IDX(length))) != NULL &&
		    m->valid != 0) {
			int base = (int)length & PAGE_MASK;
			int size = PAGE_SIZE - base;

			pmap_zero_page_area(m, base, size);

			/*
			 * Update the valid bits to reflect the blocks that
			 * have been zeroed.  Some of these valid bits may
			 * have already been set.
			 */
			vm_page_set_valid(m, base, size);

			/*
			 * Round "base" to the next block boundary so that the
			 * dirty bit for a partially zeroed block is not
			 * cleared.
			 */
			base = roundup2(base, DEV_BSIZE);

			vm_page_clear_dirty(m, base, PAGE_SIZE - base);
		} else if ((length & PAGE_MASK) &&
		    __predict_false(object->cache != NULL)) {
			vm_page_cache_free(object, OFF_TO_IDX(length),
			    nobjsize);
		}
	} else {

		/* Attempt to reserve the swap */
		delta = ptoa(nobjsize - object->size);
		if (!swap_reserve_by_cred(delta, object->cred)) {
			VM_OBJECT_UNLOCK(object);
			return (ENOMEM);
		}
		object->charge += delta;
	}
	shmfd->shm_size = length;
	mtx_lock(&shm_timestamp_lock);
	vfs_timestamp(&shmfd->shm_ctime);
	shmfd->shm_mtime = shmfd->shm_ctime;
	mtx_unlock(&shm_timestamp_lock);
	object->size = nobjsize;
	VM_OBJECT_UNLOCK(object);
	return (0);
}

/*
 * shmfd object management including creation and reference counting
 * routines.
 */
static struct shmfd *
shm_alloc(struct ucred *ucred, mode_t mode)
{
	struct shmfd *shmfd;

	shmfd = malloc(sizeof(*shmfd), M_SHMFD, M_WAITOK | M_ZERO);
	shmfd->shm_size = 0;
	shmfd->shm_uid = ucred->cr_uid;
	shmfd->shm_gid = ucred->cr_gid;
	shmfd->shm_mode = mode;
	shmfd->shm_object = vm_pager_allocate(OBJT_DEFAULT, NULL,
	    shmfd->shm_size, VM_PROT_DEFAULT, 0, ucred);
	KASSERT(shmfd->shm_object != NULL, ("shm_create: vm_pager_allocate"));
	VM_OBJECT_LOCK(shmfd->shm_object);
	vm_object_clear_flag(shmfd->shm_object, OBJ_ONEMAPPING);
	vm_object_set_flag(shmfd->shm_object, OBJ_NOSPLIT);
	VM_OBJECT_UNLOCK(shmfd->shm_object);
	vfs_timestamp(&shmfd->shm_birthtime);
	shmfd->shm_atime = shmfd->shm_mtime = shmfd->shm_ctime =
	    shmfd->shm_birthtime;
	refcount_init(&shmfd->shm_refs, 1);
#ifdef MAC
	mac_posixshm_init(shmfd);
	mac_posixshm_create(ucred, shmfd);
#endif

	return (shmfd);
}

static struct shmfd *
shm_hold(struct shmfd *shmfd)
{

	refcount_acquire(&shmfd->shm_refs);
	return (shmfd);
}

static void
shm_drop(struct shmfd *shmfd)
{

	if (refcount_release(&shmfd->shm_refs)) {
#ifdef MAC
		mac_posixshm_destroy(shmfd);
#endif
		vm_object_deallocate(shmfd->shm_object);
		free(shmfd, M_SHMFD);
	}
}

/*
 * Determine if the credentials have sufficient permissions for a
 * specified combination of FREAD and FWRITE.
 */
static int
shm_access(struct shmfd *shmfd, struct ucred *ucred, int flags)
{
	accmode_t accmode;

	accmode = 0;
	if (flags & FREAD)
		accmode |= VREAD;
	if (flags & FWRITE)
		accmode |= VWRITE;
	return (vaccess(VREG, shmfd->shm_mode, shmfd->shm_uid, shmfd->shm_gid,
	    accmode, ucred, NULL));
}

/*
 * Dictionary management.  We maintain an in-kernel dictionary to map
 * paths to shmfd objects.  We use the FNV hash on the path to store
 * the mappings in a hash table.
 */
static void
shm_dict_init(void *arg)
{

	mtx_init(&shm_timestamp_lock, "shm timestamps", NULL, MTX_DEF);
	sx_init(&shm_dict_lock, "shm dictionary");
	shm_dictionary = hashinit(1024, M_SHMFD, &shm_hash);
}
SYSINIT(shm_dict_init, SI_SUB_SYSV_SHM, SI_ORDER_ANY, shm_dict_init, NULL);

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
			LIST_REMOVE(map, sm_link);
			shm_drop(map->sm_shmfd);
			free(map->sm_path, M_SHMFD);
			free(map, M_SHMFD);
			return (0);
		}
	}

	return (ENOENT);
}

/* System calls. */
int
shm_open(struct thread *td, struct shm_open_args *uap)
{
	struct filedesc *fdp;
	struct shmfd *shmfd;
	struct file *fp;
	char *path;
	Fnv32_t fnv;
	mode_t cmode;
	int fd, error;

	if ((uap->flags & O_ACCMODE) != O_RDONLY &&
	    (uap->flags & O_ACCMODE) != O_RDWR)
		return (EINVAL);

	if ((uap->flags & ~(O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC)) != 0)
		return (EINVAL);

	fdp = td->td_proc->p_fd;
	cmode = (uap->mode & ~fdp->fd_cmask) & ACCESSPERMS;

	error = falloc(td, &fp, &fd);
	if (error)
		return (error);

	/* A SHM_ANON path pointer creates an anonymous object. */
	if (uap->path == SHM_ANON) {
		/* A read-only anonymous object is pointless. */
		if ((uap->flags & O_ACCMODE) == O_RDONLY) {
			fdclose(fdp, fp, fd, td);
			fdrop(fp, td);
			return (EINVAL);
		}
		shmfd = shm_alloc(td->td_ucred, cmode);
	} else {
		path = malloc(MAXPATHLEN, M_SHMFD, M_WAITOK);
		error = copyinstr(uap->path, path, MAXPATHLEN, NULL);

		/* Require paths to start with a '/' character. */
		if (error == 0 && path[0] != '/')
			error = EINVAL;
		if (error) {
			fdclose(fdp, fp, fd, td);
			fdrop(fp, td);
			free(path, M_SHMFD);
			return (error);
		}

		fnv = fnv_32_str(path, FNV1_32_INIT);
		sx_xlock(&shm_dict_lock);
		shmfd = shm_lookup(path, fnv);
		if (shmfd == NULL) {
			/* Object does not yet exist, create it if requested. */
			if (uap->flags & O_CREAT) {
				shmfd = shm_alloc(td->td_ucred, cmode);
				shm_insert(path, fnv, shmfd);
			} else {
				free(path, M_SHMFD);
				error = ENOENT;
			}
		} else {
			/*
			 * Object already exists, obtain a new
			 * reference if requested and permitted.
			 */
			free(path, M_SHMFD);
			if ((uap->flags & (O_CREAT | O_EXCL)) ==
			    (O_CREAT | O_EXCL))
				error = EEXIST;
			else {
#ifdef MAC
				error = mac_posixshm_check_open(td->td_ucred,
				    shmfd);
				if (error == 0)
#endif
				error = shm_access(shmfd, td->td_ucred,
				    FFLAGS(uap->flags & O_ACCMODE));
			}

			/*
			 * Truncate the file back to zero length if
			 * O_TRUNC was specified and the object was
			 * opened with read/write.
			 */
			if (error == 0 &&
			    (uap->flags & (O_ACCMODE | O_TRUNC)) ==
			    (O_RDWR | O_TRUNC)) {
#ifdef MAC
				error = mac_posixshm_check_truncate(
					td->td_ucred, fp->f_cred, shmfd);
				if (error == 0)
#endif
					shm_dotruncate(shmfd, 0);
			}
			if (error == 0)
				shm_hold(shmfd);
		}
		sx_xunlock(&shm_dict_lock);

		if (error) {
			fdclose(fdp, fp, fd, td);
			fdrop(fp, td);
			return (error);
		}
	}

	finit(fp, FFLAGS(uap->flags & O_ACCMODE), DTYPE_SHM, shmfd, &shm_ops);

	FILEDESC_XLOCK(fdp);
	if (fdp->fd_ofiles[fd] == fp)
		fdp->fd_ofileflags[fd] |= UF_EXCLOSE;
	FILEDESC_XUNLOCK(fdp);
	td->td_retval[0] = fd;
	fdrop(fp, td);

	return (0);
}

int
shm_unlink(struct thread *td, struct shm_unlink_args *uap)
{
	char *path;
	Fnv32_t fnv;
	int error;

	path = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	error = copyinstr(uap->path, path, MAXPATHLEN, NULL);
	if (error) {
		free(path, M_TEMP);
		return (error);
	}

	fnv = fnv_32_str(path, FNV1_32_INIT);
	sx_xlock(&shm_dict_lock);
	error = shm_remove(path, fnv, td->td_ucred);
	sx_xunlock(&shm_dict_lock);
	free(path, M_TEMP);

	return (error);
}

/*
 * mmap() helper to validate mmap() requests against shm object state
 * and give mmap() the vm_object to use for the mapping.
 */
int
shm_mmap(struct shmfd *shmfd, vm_size_t objsize, vm_ooffset_t foff,
    vm_object_t *obj)
{

	/*
	 * XXXRW: This validation is probably insufficient, and subject to
	 * sign errors.  It should be fixed.
	 */
	if (foff >= shmfd->shm_size ||
	    foff + objsize > round_page(shmfd->shm_size))
		return (EINVAL);

	mtx_lock(&shm_timestamp_lock);
	vfs_timestamp(&shmfd->shm_atime);
	mtx_unlock(&shm_timestamp_lock);
	vm_object_reference(shmfd->shm_object);
	*obj = shmfd->shm_object;
	return (0);
}
