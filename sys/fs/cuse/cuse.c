/* $FreeBSD$ */
/*-
 * Copyright (c) 2010-2020 Hans Petter Selasky. All rights reserved.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/sx.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/selinfo.h>
#include <sys/ptrace.h>
#include <sys/sysent.h>

#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <fs/cuse/cuse_defs.h>
#include <fs/cuse/cuse_ioctl.h>

MODULE_VERSION(cuse, 1);

/*
 * Prevent cuse4bsd.ko and cuse.ko from loading at the same time by
 * declaring support for the cuse4bsd interface in cuse.ko:
 */
MODULE_VERSION(cuse4bsd, 1);

#ifdef FEATURE
FEATURE(cuse, "Userspace character devices");
#endif

struct cuse_command;
struct cuse_server;
struct cuse_client;

struct cuse_client_command {
	TAILQ_ENTRY(cuse_client_command) entry;
	struct cuse_command sub;
	struct sx sx;
	struct cv cv;
	struct thread *entered;
	struct cuse_client *client;
	struct proc *proc_curr;
	int	proc_refs;
	int	got_signal;
	int	error;
	int	command;
};

struct cuse_memory {
	TAILQ_ENTRY(cuse_memory) entry;
	vm_object_t object;
	uint32_t page_count;
	uint32_t alloc_nr;
};

struct cuse_server_dev {
	TAILQ_ENTRY(cuse_server_dev) entry;
	struct cuse_server *server;
	struct cdev *kern_dev;
	struct cuse_dev *user_dev;
};

struct cuse_server {
	TAILQ_ENTRY(cuse_server) entry;
	TAILQ_HEAD(, cuse_client_command) head;
	TAILQ_HEAD(, cuse_server_dev) hdev;
	TAILQ_HEAD(, cuse_client) hcli;
	TAILQ_HEAD(, cuse_memory) hmem;
	struct mtx mtx;
	struct cv cv;
	struct selinfo selinfo;
	pid_t	pid;
	int	is_closing;
	int	refs;
};

struct cuse_client {
	TAILQ_ENTRY(cuse_client) entry;
	TAILQ_ENTRY(cuse_client) entry_ref;
	struct cuse_client_command cmds[CUSE_CMD_MAX];
	struct cuse_server *server;
	struct cuse_server_dev *server_dev;

	uint8_t	ioctl_buffer[CUSE_BUFFER_MAX] __aligned(4);

	int	fflags;			/* file flags */
	int	cflags;			/* client flags */
#define	CUSE_CLI_IS_CLOSING 0x01
#define	CUSE_CLI_KNOTE_NEED_READ 0x02
#define	CUSE_CLI_KNOTE_NEED_WRITE 0x04
#define	CUSE_CLI_KNOTE_HAS_READ 0x08
#define	CUSE_CLI_KNOTE_HAS_WRITE 0x10
};

#define	CUSE_CLIENT_CLOSING(pcc) \
    ((pcc)->cflags & CUSE_CLI_IS_CLOSING)

static	MALLOC_DEFINE(M_CUSE, "cuse", "CUSE memory");

static TAILQ_HEAD(, cuse_server) cuse_server_head;
static struct mtx cuse_global_mtx;
static struct cdev *cuse_dev;
static struct cuse_server *cuse_alloc_unit[CUSE_DEVICES_MAX];
static int cuse_alloc_unit_id[CUSE_DEVICES_MAX];

static void cuse_server_wakeup_all_client_locked(struct cuse_server *pcs);
static void cuse_client_kqfilter_read_detach(struct knote *kn);
static void cuse_client_kqfilter_write_detach(struct knote *kn);
static int cuse_client_kqfilter_read_event(struct knote *kn, long hint);
static int cuse_client_kqfilter_write_event(struct knote *kn, long hint);

static struct filterops cuse_client_kqfilter_read_ops = {
	.f_isfd = 1,
	.f_detach = cuse_client_kqfilter_read_detach,
	.f_event = cuse_client_kqfilter_read_event,
};

static struct filterops cuse_client_kqfilter_write_ops = {
	.f_isfd = 1,
	.f_detach = cuse_client_kqfilter_write_detach,
	.f_event = cuse_client_kqfilter_write_event,
};

static d_open_t cuse_client_open;
static d_close_t cuse_client_close;
static d_ioctl_t cuse_client_ioctl;
static d_read_t cuse_client_read;
static d_write_t cuse_client_write;
static d_poll_t cuse_client_poll;
static d_mmap_single_t cuse_client_mmap_single;
static d_kqfilter_t cuse_client_kqfilter;

static struct cdevsw cuse_client_devsw = {
	.d_version = D_VERSION,
	.d_open = cuse_client_open,
	.d_close = cuse_client_close,
	.d_ioctl = cuse_client_ioctl,
	.d_name = "cuse_client",
	.d_flags = D_TRACKCLOSE,
	.d_read = cuse_client_read,
	.d_write = cuse_client_write,
	.d_poll = cuse_client_poll,
	.d_mmap_single = cuse_client_mmap_single,
	.d_kqfilter = cuse_client_kqfilter,
};

static d_open_t cuse_server_open;
static d_close_t cuse_server_close;
static d_ioctl_t cuse_server_ioctl;
static d_read_t cuse_server_read;
static d_write_t cuse_server_write;
static d_poll_t cuse_server_poll;
static d_mmap_single_t cuse_server_mmap_single;

static struct cdevsw cuse_server_devsw = {
	.d_version = D_VERSION,
	.d_open = cuse_server_open,
	.d_close = cuse_server_close,
	.d_ioctl = cuse_server_ioctl,
	.d_name = "cuse_server",
	.d_flags = D_TRACKCLOSE,
	.d_read = cuse_server_read,
	.d_write = cuse_server_write,
	.d_poll = cuse_server_poll,
	.d_mmap_single = cuse_server_mmap_single,
};

static void cuse_client_is_closing(struct cuse_client *);
static int cuse_free_unit_by_id_locked(struct cuse_server *, int);

static void
cuse_global_lock(void)
{
	mtx_lock(&cuse_global_mtx);
}

static void
cuse_global_unlock(void)
{
	mtx_unlock(&cuse_global_mtx);
}

static void
cuse_server_lock(struct cuse_server *pcs)
{
	mtx_lock(&pcs->mtx);
}

static void
cuse_server_unlock(struct cuse_server *pcs)
{
	mtx_unlock(&pcs->mtx);
}

static void
cuse_cmd_lock(struct cuse_client_command *pccmd)
{
	sx_xlock(&pccmd->sx);
}

static void
cuse_cmd_unlock(struct cuse_client_command *pccmd)
{
	sx_xunlock(&pccmd->sx);
}

static void
cuse_kern_init(void *arg)
{
	TAILQ_INIT(&cuse_server_head);

	mtx_init(&cuse_global_mtx, "cuse-global-mtx", NULL, MTX_DEF);

	cuse_dev = make_dev(&cuse_server_devsw, 0,
	    UID_ROOT, GID_OPERATOR, 0600, "cuse");

	printf("Cuse v%d.%d.%d @ /dev/cuse\n",
	    (CUSE_VERSION >> 16) & 0xFF, (CUSE_VERSION >> 8) & 0xFF,
	    (CUSE_VERSION >> 0) & 0xFF);
}
SYSINIT(cuse_kern_init, SI_SUB_DEVFS, SI_ORDER_ANY, cuse_kern_init, NULL);

static void
cuse_kern_uninit(void *arg)
{
	void *ptr;

	while (1) {

		printf("Cuse: Please exit all /dev/cuse instances "
		    "and processes which have used this device.\n");

		pause("DRAIN", 2 * hz);

		cuse_global_lock();
		ptr = TAILQ_FIRST(&cuse_server_head);
		cuse_global_unlock();

		if (ptr == NULL)
			break;
	}

	if (cuse_dev != NULL)
		destroy_dev(cuse_dev);

	mtx_destroy(&cuse_global_mtx);
}
SYSUNINIT(cuse_kern_uninit, SI_SUB_DEVFS, SI_ORDER_ANY, cuse_kern_uninit, 0);

static int
cuse_server_get(struct cuse_server **ppcs)
{
	struct cuse_server *pcs;
	int error;

	error = devfs_get_cdevpriv((void **)&pcs);
	if (error != 0) {
		*ppcs = NULL;
		return (error);
	}
	if (pcs->is_closing) {
		*ppcs = NULL;
		return (EINVAL);
	}
	*ppcs = pcs;
	return (0);
}

static void
cuse_server_is_closing(struct cuse_server *pcs)
{
	struct cuse_client *pcc;

	if (pcs->is_closing)
		return;

	pcs->is_closing = 1;

	TAILQ_FOREACH(pcc, &pcs->hcli, entry) {
		cuse_client_is_closing(pcc);
	}
}

static struct cuse_client_command *
cuse_server_find_command(struct cuse_server *pcs, struct thread *td)
{
	struct cuse_client *pcc;
	int n;

	if (pcs->is_closing)
		goto done;

	TAILQ_FOREACH(pcc, &pcs->hcli, entry) {
		if (CUSE_CLIENT_CLOSING(pcc))
			continue;
		for (n = 0; n != CUSE_CMD_MAX; n++) {
			if (pcc->cmds[n].entered == td)
				return (&pcc->cmds[n]);
		}
	}
done:
	return (NULL);
}

static void
cuse_str_filter(char *ptr)
{
	int c;

	while (((c = *ptr) != 0)) {

		if ((c >= 'a') && (c <= 'z')) {
			ptr++;
			continue;
		}
		if ((c >= 'A') && (c <= 'Z')) {
			ptr++;
			continue;
		}
		if ((c >= '0') && (c <= '9')) {
			ptr++;
			continue;
		}
		if ((c == '.') || (c == '_') || (c == '/')) {
			ptr++;
			continue;
		}
		*ptr = '_';

		ptr++;
	}
}

static int
cuse_convert_error(int error)
{
	;				/* indent fix */
	switch (error) {
	case CUSE_ERR_NONE:
		return (0);
	case CUSE_ERR_BUSY:
		return (EBUSY);
	case CUSE_ERR_WOULDBLOCK:
		return (EWOULDBLOCK);
	case CUSE_ERR_INVALID:
		return (EINVAL);
	case CUSE_ERR_NO_MEMORY:
		return (ENOMEM);
	case CUSE_ERR_FAULT:
		return (EFAULT);
	case CUSE_ERR_SIGNAL:
		return (EINTR);
	case CUSE_ERR_NO_DEVICE:
		return (ENODEV);
	default:
		return (ENXIO);
	}
}

static void
cuse_vm_memory_free(struct cuse_memory *mem)
{
	/* last user is gone - free */
	vm_object_deallocate(mem->object);

	/* free CUSE memory */
	free(mem, M_CUSE);
}

static int
cuse_server_alloc_memory(struct cuse_server *pcs, uint32_t alloc_nr,
    uint32_t page_count)
{
	struct cuse_memory *temp;
	struct cuse_memory *mem;
	vm_object_t object;
	int error;

	mem = malloc(sizeof(*mem), M_CUSE, M_WAITOK | M_ZERO);

	object = vm_pager_allocate(OBJT_SWAP, NULL, PAGE_SIZE * page_count,
	    VM_PROT_DEFAULT, 0, curthread->td_ucred);
	if (object == NULL) {
		error = ENOMEM;
		goto error_0;
	}

	cuse_server_lock(pcs);
	/* check if allocation number already exists */
	TAILQ_FOREACH(temp, &pcs->hmem, entry) {
		if (temp->alloc_nr == alloc_nr)
			break;
	}
	if (temp != NULL) {
		cuse_server_unlock(pcs);
		error = EBUSY;
		goto error_1;
	}
	mem->object = object;
	mem->page_count = page_count;
	mem->alloc_nr = alloc_nr;
	TAILQ_INSERT_TAIL(&pcs->hmem, mem, entry);
	cuse_server_unlock(pcs);

	return (0);

error_1:
	vm_object_deallocate(object);
error_0:
	free(mem, M_CUSE);
	return (error);
}

static int
cuse_server_free_memory(struct cuse_server *pcs, uint32_t alloc_nr)
{
	struct cuse_memory *mem;

	cuse_server_lock(pcs);
	TAILQ_FOREACH(mem, &pcs->hmem, entry) {
		if (mem->alloc_nr == alloc_nr)
			break;
	}
	if (mem == NULL) {
		cuse_server_unlock(pcs);
		return (EINVAL);
	}
	TAILQ_REMOVE(&pcs->hmem, mem, entry);
	cuse_server_unlock(pcs);

	cuse_vm_memory_free(mem);

	return (0);
}

static int
cuse_client_get(struct cuse_client **ppcc)
{
	struct cuse_client *pcc;
	int error;

	/* try to get private data */
	error = devfs_get_cdevpriv((void **)&pcc);
	if (error != 0) {
		*ppcc = NULL;
		return (error);
	}
	if (CUSE_CLIENT_CLOSING(pcc) || pcc->server->is_closing) {
		*ppcc = NULL;
		return (EINVAL);
	}
	*ppcc = pcc;
	return (0);
}

static void
cuse_client_is_closing(struct cuse_client *pcc)
{
	struct cuse_client_command *pccmd;
	uint32_t n;

	if (CUSE_CLIENT_CLOSING(pcc))
		return;

	pcc->cflags |= CUSE_CLI_IS_CLOSING;
	pcc->server_dev = NULL;

	for (n = 0; n != CUSE_CMD_MAX; n++) {

		pccmd = &pcc->cmds[n];

		if (pccmd->entry.tqe_prev != NULL) {
			TAILQ_REMOVE(&pcc->server->head, pccmd, entry);
			pccmd->entry.tqe_prev = NULL;
		}
		cv_broadcast(&pccmd->cv);
	}
}

static void
cuse_client_send_command_locked(struct cuse_client_command *pccmd,
    uintptr_t data_ptr, unsigned long arg, int fflags, int ioflag)
{
	unsigned long cuse_fflags = 0;
	struct cuse_server *pcs;

	if (fflags & FREAD)
		cuse_fflags |= CUSE_FFLAG_READ;

	if (fflags & FWRITE)
		cuse_fflags |= CUSE_FFLAG_WRITE;

	if (ioflag & IO_NDELAY)
		cuse_fflags |= CUSE_FFLAG_NONBLOCK;
#if defined(__LP64__)
	if (SV_CURPROC_FLAG(SV_ILP32))
		cuse_fflags |= CUSE_FFLAG_COMPAT32;
#endif
	pccmd->sub.fflags = cuse_fflags;
	pccmd->sub.data_pointer = data_ptr;
	pccmd->sub.argument = arg;

	pcs = pccmd->client->server;

	if ((pccmd->entry.tqe_prev == NULL) &&
	    (CUSE_CLIENT_CLOSING(pccmd->client) == 0) &&
	    (pcs->is_closing == 0)) {
		TAILQ_INSERT_TAIL(&pcs->head, pccmd, entry);
		cv_signal(&pcs->cv);
	}
}

static void
cuse_client_got_signal(struct cuse_client_command *pccmd)
{
	struct cuse_server *pcs;

	pccmd->got_signal = 1;

	pccmd = &pccmd->client->cmds[CUSE_CMD_SIGNAL];

	pcs = pccmd->client->server;

	if ((pccmd->entry.tqe_prev == NULL) &&
	    (CUSE_CLIENT_CLOSING(pccmd->client) == 0) &&
	    (pcs->is_closing == 0)) {
		TAILQ_INSERT_TAIL(&pcs->head, pccmd, entry);
		cv_signal(&pcs->cv);
	}
}

static int
cuse_client_receive_command_locked(struct cuse_client_command *pccmd,
    uint8_t *arg_ptr, uint32_t arg_len)
{
	struct cuse_server *pcs;
	int error;

	pcs = pccmd->client->server;
	error = 0;

	pccmd->proc_curr = curthread->td_proc;

	if (CUSE_CLIENT_CLOSING(pccmd->client) || pcs->is_closing) {
		error = CUSE_ERR_OTHER;
		goto done;
	}
	while (pccmd->command == CUSE_CMD_NONE) {
		if (error != 0) {
			cv_wait(&pccmd->cv, &pcs->mtx);
		} else {
			error = cv_wait_sig(&pccmd->cv, &pcs->mtx);

			if (error != 0)
				cuse_client_got_signal(pccmd);
		}
		if (CUSE_CLIENT_CLOSING(pccmd->client) || pcs->is_closing) {
			error = CUSE_ERR_OTHER;
			goto done;
		}
	}

	error = pccmd->error;
	pccmd->command = CUSE_CMD_NONE;
	cv_signal(&pccmd->cv);

done:

	/* wait until all process references are gone */

	pccmd->proc_curr = NULL;

	while (pccmd->proc_refs != 0)
		cv_wait(&pccmd->cv, &pcs->mtx);

	return (error);
}

/*------------------------------------------------------------------------*
 *	CUSE SERVER PART
 *------------------------------------------------------------------------*/

static void
cuse_server_free_dev(struct cuse_server_dev *pcsd)
{
	struct cuse_server *pcs;
	struct cuse_client *pcc;

	/* get server pointer */
	pcs = pcsd->server;

	/* prevent creation of more devices */
	cuse_server_lock(pcs);
	if (pcsd->kern_dev != NULL)
		pcsd->kern_dev->si_drv1 = NULL;

	TAILQ_FOREACH(pcc, &pcs->hcli, entry) {
		if (pcc->server_dev == pcsd)
			cuse_client_is_closing(pcc);
	}
	cuse_server_unlock(pcs);

	/* destroy device, if any */
	if (pcsd->kern_dev != NULL) {
		/* destroy device synchronously */
		destroy_dev(pcsd->kern_dev);
	}
	free(pcsd, M_CUSE);
}

static void
cuse_server_unref(struct cuse_server *pcs)
{
	struct cuse_server_dev *pcsd;
	struct cuse_memory *mem;

	cuse_server_lock(pcs);
	if (--(pcs->refs) != 0) {
		cuse_server_unlock(pcs);
		return;
	}
	cuse_server_is_closing(pcs);
	/* final client wakeup, if any */
	cuse_server_wakeup_all_client_locked(pcs);

	cuse_global_lock();
	TAILQ_REMOVE(&cuse_server_head, pcs, entry);
	cuse_global_unlock();

	while ((pcsd = TAILQ_FIRST(&pcs->hdev)) != NULL) {
		TAILQ_REMOVE(&pcs->hdev, pcsd, entry);
		cuse_server_unlock(pcs);
		cuse_server_free_dev(pcsd);
		cuse_server_lock(pcs);
	}

	cuse_free_unit_by_id_locked(pcs, -1);

	while ((mem = TAILQ_FIRST(&pcs->hmem)) != NULL) {
		TAILQ_REMOVE(&pcs->hmem, mem, entry);
		cuse_server_unlock(pcs);
		cuse_vm_memory_free(mem);
		cuse_server_lock(pcs);
	}

	knlist_clear(&pcs->selinfo.si_note, 1);
	knlist_destroy(&pcs->selinfo.si_note);

	cuse_server_unlock(pcs);

	seldrain(&pcs->selinfo);

	cv_destroy(&pcs->cv);

	mtx_destroy(&pcs->mtx);

	free(pcs, M_CUSE);
}

static int
cuse_server_do_close(struct cuse_server *pcs)
{
	int retval;

	cuse_server_lock(pcs);
	cuse_server_is_closing(pcs);
	/* final client wakeup, if any */
	cuse_server_wakeup_all_client_locked(pcs);

	knlist_clear(&pcs->selinfo.si_note, 1);

	retval = pcs->refs;
	cuse_server_unlock(pcs);

	return (retval);
}

static void
cuse_server_free(void *arg)
{
	struct cuse_server *pcs = arg;

	/*
	 * The final server unref should be done by the server thread
	 * to prevent deadlock in the client cdevpriv destructor,
	 * which cannot destroy itself.
	 */
	while (cuse_server_do_close(pcs) != 1)
		pause("W", hz);

	/* drop final refcount */
	cuse_server_unref(pcs);
}

static int
cuse_server_open(struct cdev *dev, int fflags, int devtype, struct thread *td)
{
	struct cuse_server *pcs;

	pcs = malloc(sizeof(*pcs), M_CUSE, M_WAITOK | M_ZERO);

	if (devfs_set_cdevpriv(pcs, &cuse_server_free)) {
		printf("Cuse: Cannot set cdevpriv.\n");
		free(pcs, M_CUSE);
		return (ENOMEM);
	}
	/* store current process ID */
	pcs->pid = curproc->p_pid;

	TAILQ_INIT(&pcs->head);
	TAILQ_INIT(&pcs->hdev);
	TAILQ_INIT(&pcs->hcli);
	TAILQ_INIT(&pcs->hmem);

	cv_init(&pcs->cv, "cuse-server-cv");

	mtx_init(&pcs->mtx, "cuse-server-mtx", NULL, MTX_DEF);

	knlist_init_mtx(&pcs->selinfo.si_note, &pcs->mtx);

	cuse_global_lock();
	pcs->refs++;
	TAILQ_INSERT_TAIL(&cuse_server_head, pcs, entry);
	cuse_global_unlock();

	return (0);
}

static int
cuse_server_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct cuse_server *pcs;

	if (cuse_server_get(&pcs) == 0)
		cuse_server_do_close(pcs);

	return (0);
}

static int
cuse_server_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	return (ENXIO);
}

static int
cuse_server_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	return (ENXIO);
}

static int
cuse_server_ioctl_copy_locked(struct cuse_server *pcs,
    struct cuse_client_command *pccmd,
    struct cuse_data_chunk *pchk, int isread)
{
	struct proc *p_proc;
	uint32_t offset;
	int error;

	offset = pchk->peer_ptr - CUSE_BUF_MIN_PTR;

	if (pchk->length > CUSE_BUFFER_MAX)
		return (EFAULT);

	if (offset >= CUSE_BUFFER_MAX)
		return (EFAULT);

	if ((offset + pchk->length) > CUSE_BUFFER_MAX)
		return (EFAULT);

	p_proc = pccmd->proc_curr;
	if (p_proc == NULL)
		return (ENXIO);

	if (pccmd->proc_refs < 0)
		return (ENOMEM);

	pccmd->proc_refs++;

	cuse_server_unlock(pcs);

	if (isread == 0) {
		error = copyin(
		    (void *)pchk->local_ptr,
		    pccmd->client->ioctl_buffer + offset,
		    pchk->length);
	} else {
		error = copyout(
		    pccmd->client->ioctl_buffer + offset,
		    (void *)pchk->local_ptr,
		    pchk->length);
	}

	cuse_server_lock(pcs);

	pccmd->proc_refs--;

	if (pccmd->proc_curr == NULL)
		cv_signal(&pccmd->cv);

	return (error);
}

static int
cuse_proc2proc_copy(struct proc *proc_s, vm_offset_t data_s,
    struct proc *proc_d, vm_offset_t data_d, size_t len)
{
	struct thread *td;
	struct proc *proc_cur;
	int error;

	td = curthread;
	proc_cur = td->td_proc;

	if (proc_cur == proc_d) {
		struct iovec iov = {
			.iov_base = (caddr_t)data_d,
			.iov_len = len,
		};
		struct uio uio = {
			.uio_iov = &iov,
			.uio_iovcnt = 1,
			.uio_offset = (off_t)data_s,
			.uio_resid = len,
			.uio_segflg = UIO_USERSPACE,
			.uio_rw = UIO_READ,
			.uio_td = td,
		};

		PHOLD(proc_s);
		error = proc_rwmem(proc_s, &uio);
		PRELE(proc_s);

	} else if (proc_cur == proc_s) {
		struct iovec iov = {
			.iov_base = (caddr_t)data_s,
			.iov_len = len,
		};
		struct uio uio = {
			.uio_iov = &iov,
			.uio_iovcnt = 1,
			.uio_offset = (off_t)data_d,
			.uio_resid = len,
			.uio_segflg = UIO_USERSPACE,
			.uio_rw = UIO_WRITE,
			.uio_td = td,
		};

		PHOLD(proc_d);
		error = proc_rwmem(proc_d, &uio);
		PRELE(proc_d);
	} else {
		error = EINVAL;
	}
	return (error);
}

static int
cuse_server_data_copy_locked(struct cuse_server *pcs,
    struct cuse_client_command *pccmd,
    struct cuse_data_chunk *pchk, int isread)
{
	struct proc *p_proc;
	int error;

	p_proc = pccmd->proc_curr;
	if (p_proc == NULL)
		return (ENXIO);

	if (pccmd->proc_refs < 0)
		return (ENOMEM);

	pccmd->proc_refs++;

	cuse_server_unlock(pcs);

	if (isread == 0) {
		error = cuse_proc2proc_copy(
		    curthread->td_proc, pchk->local_ptr,
		    p_proc, pchk->peer_ptr,
		    pchk->length);
	} else {
		error = cuse_proc2proc_copy(
		    p_proc, pchk->peer_ptr,
		    curthread->td_proc, pchk->local_ptr,
		    pchk->length);
	}

	cuse_server_lock(pcs);

	pccmd->proc_refs--;

	if (pccmd->proc_curr == NULL)
		cv_signal(&pccmd->cv);

	return (error);
}

static int
cuse_alloc_unit_by_id_locked(struct cuse_server *pcs, int id)
{
	int n;
	int x = 0;
	int match;

	do {
		for (match = n = 0; n != CUSE_DEVICES_MAX; n++) {
			if (cuse_alloc_unit[n] != NULL) {
				if ((cuse_alloc_unit_id[n] ^ id) & CUSE_ID_MASK)
					continue;
				if ((cuse_alloc_unit_id[n] & ~CUSE_ID_MASK) == x) {
					x++;
					match = 1;
				}
			}
		}
	} while (match);

	if (x < 256) {
		for (n = 0; n != CUSE_DEVICES_MAX; n++) {
			if (cuse_alloc_unit[n] == NULL) {
				cuse_alloc_unit[n] = pcs;
				cuse_alloc_unit_id[n] = id | x;
				return (x);
			}
		}
	}
	return (-1);
}

static void
cuse_server_wakeup_locked(struct cuse_server *pcs)
{
	selwakeup(&pcs->selinfo);
	KNOTE_LOCKED(&pcs->selinfo.si_note, 0);
}

static void
cuse_server_wakeup_all_client_locked(struct cuse_server *pcs)
{
	struct cuse_client *pcc;

	TAILQ_FOREACH(pcc, &pcs->hcli, entry) {
		pcc->cflags |= (CUSE_CLI_KNOTE_NEED_READ |
		    CUSE_CLI_KNOTE_NEED_WRITE);
	}
	cuse_server_wakeup_locked(pcs);
}

static int
cuse_free_unit_by_id_locked(struct cuse_server *pcs, int id)
{
	int n;
	int found = 0;

	for (n = 0; n != CUSE_DEVICES_MAX; n++) {
		if (cuse_alloc_unit[n] == pcs) {
			if (cuse_alloc_unit_id[n] == id || id == -1) {
				cuse_alloc_unit[n] = NULL;
				cuse_alloc_unit_id[n] = 0;
				found = 1;
			}
		}
	}

	return (found ? 0 : EINVAL);
}

static int
cuse_server_ioctl(struct cdev *dev, unsigned long cmd,
    caddr_t data, int fflag, struct thread *td)
{
	struct cuse_server *pcs;
	int error;

	error = cuse_server_get(&pcs);
	if (error != 0)
		return (error);

	switch (cmd) {
		struct cuse_client_command *pccmd;
		struct cuse_client *pcc;
		struct cuse_command *pcmd;
		struct cuse_alloc_info *pai;
		struct cuse_create_dev *pcd;
		struct cuse_server_dev *pcsd;
		struct cuse_data_chunk *pchk;
		int n;

	case CUSE_IOCTL_GET_COMMAND:
		pcmd = (void *)data;

		cuse_server_lock(pcs);

		while ((pccmd = TAILQ_FIRST(&pcs->head)) == NULL) {
			error = cv_wait_sig(&pcs->cv, &pcs->mtx);

			if (pcs->is_closing)
				error = ENXIO;

			if (error) {
				cuse_server_unlock(pcs);
				return (error);
			}
		}

		TAILQ_REMOVE(&pcs->head, pccmd, entry);
		pccmd->entry.tqe_prev = NULL;

		pccmd->entered = curthread;

		*pcmd = pccmd->sub;

		cuse_server_unlock(pcs);

		break;

	case CUSE_IOCTL_SYNC_COMMAND:

		cuse_server_lock(pcs);
		while ((pccmd = cuse_server_find_command(pcs, curthread)) != NULL) {

			/* send sync command */
			pccmd->entered = NULL;
			pccmd->error = *(int *)data;
			pccmd->command = CUSE_CMD_SYNC;

			/* signal peer, if any */
			cv_signal(&pccmd->cv);
		}
		cuse_server_unlock(pcs);

		break;

	case CUSE_IOCTL_ALLOC_UNIT:

		cuse_server_lock(pcs);
		n = cuse_alloc_unit_by_id_locked(pcs,
		    CUSE_ID_DEFAULT(0));
		cuse_server_unlock(pcs);

		if (n < 0)
			error = ENOMEM;
		else
			*(int *)data = n;
		break;

	case CUSE_IOCTL_ALLOC_UNIT_BY_ID:

		n = *(int *)data;

		n = (n & CUSE_ID_MASK);

		cuse_server_lock(pcs);
		n = cuse_alloc_unit_by_id_locked(pcs, n);
		cuse_server_unlock(pcs);

		if (n < 0)
			error = ENOMEM;
		else
			*(int *)data = n;
		break;

	case CUSE_IOCTL_FREE_UNIT:

		n = *(int *)data;

		n = CUSE_ID_DEFAULT(n);

		cuse_server_lock(pcs);
		error = cuse_free_unit_by_id_locked(pcs, n);
		cuse_server_unlock(pcs);
		break;

	case CUSE_IOCTL_FREE_UNIT_BY_ID:

		n = *(int *)data;

		cuse_server_lock(pcs);
		error = cuse_free_unit_by_id_locked(pcs, n);
		cuse_server_unlock(pcs);
		break;

	case CUSE_IOCTL_ALLOC_MEMORY:

		pai = (void *)data;

		if (pai->alloc_nr >= CUSE_ALLOC_UNIT_MAX) {
			error = ENOMEM;
			break;
		}
		if (pai->page_count >= CUSE_ALLOC_PAGES_MAX) {
			error = ENOMEM;
			break;
		}
		error = cuse_server_alloc_memory(pcs,
		    pai->alloc_nr, pai->page_count);
		break;

	case CUSE_IOCTL_FREE_MEMORY:
		pai = (void *)data;

		if (pai->alloc_nr >= CUSE_ALLOC_UNIT_MAX) {
			error = ENOMEM;
			break;
		}
		error = cuse_server_free_memory(pcs, pai->alloc_nr);
		break;

	case CUSE_IOCTL_GET_SIG:

		cuse_server_lock(pcs);
		pccmd = cuse_server_find_command(pcs, curthread);

		if (pccmd != NULL) {
			n = pccmd->got_signal;
			pccmd->got_signal = 0;
		} else {
			n = 0;
		}
		cuse_server_unlock(pcs);

		*(int *)data = n;

		break;

	case CUSE_IOCTL_SET_PFH:

		cuse_server_lock(pcs);
		pccmd = cuse_server_find_command(pcs, curthread);

		if (pccmd != NULL) {
			pcc = pccmd->client;
			for (n = 0; n != CUSE_CMD_MAX; n++) {
				pcc->cmds[n].sub.per_file_handle = *(uintptr_t *)data;
			}
		} else {
			error = ENXIO;
		}
		cuse_server_unlock(pcs);
		break;

	case CUSE_IOCTL_CREATE_DEV:

		error = priv_check(curthread, PRIV_DRIVER);
		if (error)
			break;

		pcd = (void *)data;

		/* filter input */

		pcd->devname[sizeof(pcd->devname) - 1] = 0;

		if (pcd->devname[0] == 0) {
			error = EINVAL;
			break;
		}
		cuse_str_filter(pcd->devname);

		pcd->permissions &= 0777;

		/* try to allocate a character device */

		pcsd = malloc(sizeof(*pcsd), M_CUSE, M_WAITOK | M_ZERO);

		pcsd->server = pcs;

		pcsd->user_dev = pcd->dev;

		pcsd->kern_dev = make_dev_credf(MAKEDEV_CHECKNAME,
		    &cuse_client_devsw, 0, NULL, pcd->user_id, pcd->group_id,
		    pcd->permissions, "%s", pcd->devname);

		if (pcsd->kern_dev == NULL) {
			free(pcsd, M_CUSE);
			error = ENOMEM;
			break;
		}
		pcsd->kern_dev->si_drv1 = pcsd;

		cuse_server_lock(pcs);
		TAILQ_INSERT_TAIL(&pcs->hdev, pcsd, entry);
		cuse_server_unlock(pcs);

		break;

	case CUSE_IOCTL_DESTROY_DEV:

		error = priv_check(curthread, PRIV_DRIVER);
		if (error)
			break;

		cuse_server_lock(pcs);

		error = EINVAL;

		pcsd = TAILQ_FIRST(&pcs->hdev);
		while (pcsd != NULL) {
			if (pcsd->user_dev == *(struct cuse_dev **)data) {
				TAILQ_REMOVE(&pcs->hdev, pcsd, entry);
				cuse_server_unlock(pcs);
				cuse_server_free_dev(pcsd);
				cuse_server_lock(pcs);
				error = 0;
				pcsd = TAILQ_FIRST(&pcs->hdev);
			} else {
				pcsd = TAILQ_NEXT(pcsd, entry);
			}
		}

		cuse_server_unlock(pcs);
		break;

	case CUSE_IOCTL_WRITE_DATA:
	case CUSE_IOCTL_READ_DATA:

		cuse_server_lock(pcs);
		pchk = (struct cuse_data_chunk *)data;

		pccmd = cuse_server_find_command(pcs, curthread);

		if (pccmd == NULL) {
			error = ENXIO;	/* invalid request */
		} else if (pchk->peer_ptr < CUSE_BUF_MIN_PTR) {
			error = EFAULT;	/* NULL pointer */
		} else if (pchk->peer_ptr < CUSE_BUF_MAX_PTR) {
			error = cuse_server_ioctl_copy_locked(pcs, pccmd,
			    pchk, cmd == CUSE_IOCTL_READ_DATA);
		} else {
			error = cuse_server_data_copy_locked(pcs, pccmd,
			    pchk, cmd == CUSE_IOCTL_READ_DATA);
		}
		cuse_server_unlock(pcs);
		break;

	case CUSE_IOCTL_SELWAKEUP:
		cuse_server_lock(pcs);
		/*
		 * We don't know which direction caused the event.
		 * Wakeup both!
		 */
		cuse_server_wakeup_all_client_locked(pcs);
		cuse_server_unlock(pcs);
		break;

	default:
		error = ENXIO;
		break;
	}
	return (error);
}

static int
cuse_server_poll(struct cdev *dev, int events, struct thread *td)
{
	return (events & (POLLHUP | POLLPRI | POLLIN |
	    POLLRDNORM | POLLOUT | POLLWRNORM));
}

static int
cuse_server_mmap_single(struct cdev *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	uint32_t page_nr = *offset / PAGE_SIZE;
	uint32_t alloc_nr = page_nr / CUSE_ALLOC_PAGES_MAX;
	struct cuse_memory *mem;
	struct cuse_server *pcs;
	int error;

	error = cuse_server_get(&pcs);
	if (error != 0)
		return (error);

	cuse_server_lock(pcs);
	/* lookup memory structure */
	TAILQ_FOREACH(mem, &pcs->hmem, entry) {
		if (mem->alloc_nr == alloc_nr)
			break;
	}
	if (mem == NULL) {
		cuse_server_unlock(pcs);
		return (ENOMEM);
	}
	/* verify page offset */
	page_nr %= CUSE_ALLOC_PAGES_MAX;
	if (page_nr >= mem->page_count) {
		cuse_server_unlock(pcs);
		return (ENXIO);
	}
	/* verify mmap size */
	if ((size % PAGE_SIZE) != 0 || (size < PAGE_SIZE) ||
	    (size > ((mem->page_count - page_nr) * PAGE_SIZE))) {
		cuse_server_unlock(pcs);
		return (EINVAL);
	}
	vm_object_reference(mem->object);
	*object = mem->object;
	cuse_server_unlock(pcs);

	/* set new VM object offset to use */
	*offset = page_nr * PAGE_SIZE;

	/* success */
	return (0);
}

/*------------------------------------------------------------------------*
 *	CUSE CLIENT PART
 *------------------------------------------------------------------------*/
static void
cuse_client_free(void *arg)
{
	struct cuse_client *pcc = arg;
	struct cuse_client_command *pccmd;
	struct cuse_server *pcs;
	int n;

	pcs = pcc->server;

	cuse_server_lock(pcs);
	cuse_client_is_closing(pcc);
	TAILQ_REMOVE(&pcs->hcli, pcc, entry);
	cuse_server_unlock(pcs);

	for (n = 0; n != CUSE_CMD_MAX; n++) {

		pccmd = &pcc->cmds[n];

		sx_destroy(&pccmd->sx);
		cv_destroy(&pccmd->cv);
	}

	free(pcc, M_CUSE);

	/* drop reference on server */
	cuse_server_unref(pcs);
}

static int
cuse_client_open(struct cdev *dev, int fflags, int devtype, struct thread *td)
{
	struct cuse_client_command *pccmd;
	struct cuse_server_dev *pcsd;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	struct cuse_dev *pcd;
	int error;
	int n;

	pcsd = dev->si_drv1;
	if (pcsd != NULL) {
		pcs = pcsd->server;
		pcd = pcsd->user_dev;

		cuse_server_lock(pcs);
		/*
		 * Check that the refcount didn't wrap and that the
		 * same process is not both client and server. This
		 * can easily lead to deadlocks when destroying the
		 * CUSE character device nodes:
		 */
		pcs->refs++;
		if (pcs->refs < 0 || pcs->pid == curproc->p_pid) {
			/* overflow or wrong PID */
			pcs->refs--;
			cuse_server_unlock(pcs);
			return (EINVAL);
		}
		cuse_server_unlock(pcs);
	} else {
		return (EINVAL);
	}

	pcc = malloc(sizeof(*pcc), M_CUSE, M_WAITOK | M_ZERO);
	if (devfs_set_cdevpriv(pcc, &cuse_client_free)) {
		printf("Cuse: Cannot set cdevpriv.\n");
		/* drop reference on server */
		cuse_server_unref(pcs);
		free(pcc, M_CUSE);
		return (ENOMEM);
	}
	pcc->fflags = fflags;
	pcc->server_dev = pcsd;
	pcc->server = pcs;

	for (n = 0; n != CUSE_CMD_MAX; n++) {

		pccmd = &pcc->cmds[n];

		pccmd->sub.dev = pcd;
		pccmd->sub.command = n;
		pccmd->client = pcc;

		sx_init(&pccmd->sx, "cuse-client-sx");
		cv_init(&pccmd->cv, "cuse-client-cv");
	}

	cuse_server_lock(pcs);

	/* cuse_client_free() assumes that the client is listed somewhere! */
	/* always enqueue */

	TAILQ_INSERT_TAIL(&pcs->hcli, pcc, entry);

	/* check if server is closing */
	if ((pcs->is_closing != 0) || (dev->si_drv1 == NULL)) {
		error = EINVAL;
	} else {
		error = 0;
	}
	cuse_server_unlock(pcs);

	if (error) {
		devfs_clear_cdevpriv();	/* XXX bugfix */
		return (error);
	}
	pccmd = &pcc->cmds[CUSE_CMD_OPEN];

	cuse_cmd_lock(pccmd);

	cuse_server_lock(pcs);
	cuse_client_send_command_locked(pccmd, 0, 0, pcc->fflags, 0);

	error = cuse_client_receive_command_locked(pccmd, 0, 0);
	cuse_server_unlock(pcs);

	if (error < 0) {
		error = cuse_convert_error(error);
	} else {
		error = 0;
	}

	cuse_cmd_unlock(pccmd);

	if (error)
		devfs_clear_cdevpriv();	/* XXX bugfix */

	return (error);
}

static int
cuse_client_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct cuse_client_command *pccmd;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	int error;

	error = cuse_client_get(&pcc);
	if (error != 0)
		return (0);

	pccmd = &pcc->cmds[CUSE_CMD_CLOSE];
	pcs = pcc->server;

	cuse_cmd_lock(pccmd);

	cuse_server_lock(pcs);
	cuse_client_send_command_locked(pccmd, 0, 0, pcc->fflags, 0);

	error = cuse_client_receive_command_locked(pccmd, 0, 0);
	cuse_cmd_unlock(pccmd);

	cuse_client_is_closing(pcc);
	cuse_server_unlock(pcs);

	return (0);
}

static void
cuse_client_kqfilter_poll(struct cdev *dev, struct cuse_client *pcc)
{
	struct cuse_server *pcs = pcc->server;
	int temp;

	cuse_server_lock(pcs);
	temp = (pcc->cflags & (CUSE_CLI_KNOTE_HAS_READ |
	    CUSE_CLI_KNOTE_HAS_WRITE));
	pcc->cflags &= ~(CUSE_CLI_KNOTE_NEED_READ |
	    CUSE_CLI_KNOTE_NEED_WRITE);
	cuse_server_unlock(pcs);

	if (temp != 0) {
		/* get the latest polling state from the server */
		temp = cuse_client_poll(dev, POLLIN | POLLOUT, NULL);

		if (temp & (POLLIN | POLLOUT)) {
			cuse_server_lock(pcs);
			if (temp & POLLIN)
				pcc->cflags |= CUSE_CLI_KNOTE_NEED_READ;
			if (temp & POLLOUT)
				pcc->cflags |= CUSE_CLI_KNOTE_NEED_WRITE;

			/* make sure the "knote" gets woken up */
			cuse_server_wakeup_locked(pcc->server);
			cuse_server_unlock(pcs);
		}
	}
}

static int
cuse_client_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct cuse_client_command *pccmd;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	int error;
	int len;

	error = cuse_client_get(&pcc);
	if (error != 0)
		return (error);

	pccmd = &pcc->cmds[CUSE_CMD_READ];
	pcs = pcc->server;

	if (uio->uio_segflg != UIO_USERSPACE) {
		return (EINVAL);
	}
	uio->uio_segflg = UIO_NOCOPY;

	cuse_cmd_lock(pccmd);

	while (uio->uio_resid != 0) {

		if (uio->uio_iov->iov_len > CUSE_LENGTH_MAX) {
			error = ENOMEM;
			break;
		}
		len = uio->uio_iov->iov_len;

		cuse_server_lock(pcs);
		cuse_client_send_command_locked(pccmd,
		    (uintptr_t)uio->uio_iov->iov_base,
		    (unsigned long)(unsigned int)len, pcc->fflags, ioflag);

		error = cuse_client_receive_command_locked(pccmd, 0, 0);
		cuse_server_unlock(pcs);

		if (error < 0) {
			error = cuse_convert_error(error);
			break;
		} else if (error == len) {
			error = uiomove(NULL, error, uio);
			if (error)
				break;
		} else {
			error = uiomove(NULL, error, uio);
			break;
		}
	}
	cuse_cmd_unlock(pccmd);

	uio->uio_segflg = UIO_USERSPACE;/* restore segment flag */

	if (error == EWOULDBLOCK)
		cuse_client_kqfilter_poll(dev, pcc);

	return (error);
}

static int
cuse_client_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct cuse_client_command *pccmd;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	int error;
	int len;

	error = cuse_client_get(&pcc);
	if (error != 0)
		return (error);

	pccmd = &pcc->cmds[CUSE_CMD_WRITE];
	pcs = pcc->server;

	if (uio->uio_segflg != UIO_USERSPACE) {
		return (EINVAL);
	}
	uio->uio_segflg = UIO_NOCOPY;

	cuse_cmd_lock(pccmd);

	while (uio->uio_resid != 0) {

		if (uio->uio_iov->iov_len > CUSE_LENGTH_MAX) {
			error = ENOMEM;
			break;
		}
		len = uio->uio_iov->iov_len;

		cuse_server_lock(pcs);
		cuse_client_send_command_locked(pccmd,
		    (uintptr_t)uio->uio_iov->iov_base,
		    (unsigned long)(unsigned int)len, pcc->fflags, ioflag);

		error = cuse_client_receive_command_locked(pccmd, 0, 0);
		cuse_server_unlock(pcs);

		if (error < 0) {
			error = cuse_convert_error(error);
			break;
		} else if (error == len) {
			error = uiomove(NULL, error, uio);
			if (error)
				break;
		} else {
			error = uiomove(NULL, error, uio);
			break;
		}
	}
	cuse_cmd_unlock(pccmd);

	uio->uio_segflg = UIO_USERSPACE;/* restore segment flag */

	if (error == EWOULDBLOCK)
		cuse_client_kqfilter_poll(dev, pcc);

	return (error);
}

int
cuse_client_ioctl(struct cdev *dev, unsigned long cmd,
    caddr_t data, int fflag, struct thread *td)
{
	struct cuse_client_command *pccmd;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	int error;
	int len;

	error = cuse_client_get(&pcc);
	if (error != 0)
		return (error);

	len = IOCPARM_LEN(cmd);
	if (len > CUSE_BUFFER_MAX)
		return (ENOMEM);

	pccmd = &pcc->cmds[CUSE_CMD_IOCTL];
	pcs = pcc->server;

	cuse_cmd_lock(pccmd);

	if (cmd & (IOC_IN | IOC_VOID))
		memcpy(pcc->ioctl_buffer, data, len);

	/*
	 * When the ioctl-length is zero drivers can pass information
	 * through the data pointer of the ioctl. Make sure this information
	 * is forwarded to the driver.
	 */

	cuse_server_lock(pcs);
	cuse_client_send_command_locked(pccmd,
	    (len == 0) ? *(long *)data : CUSE_BUF_MIN_PTR,
	    (unsigned long)cmd, pcc->fflags,
	    (fflag & O_NONBLOCK) ? IO_NDELAY : 0);

	error = cuse_client_receive_command_locked(pccmd, data, len);
	cuse_server_unlock(pcs);

	if (error < 0) {
		error = cuse_convert_error(error);
	} else {
		error = 0;
	}

	if (cmd & IOC_OUT)
		memcpy(data, pcc->ioctl_buffer, len);

	cuse_cmd_unlock(pccmd);

	if (error == EWOULDBLOCK)
		cuse_client_kqfilter_poll(dev, pcc);

	return (error);
}

static int
cuse_client_poll(struct cdev *dev, int events, struct thread *td)
{
	struct cuse_client_command *pccmd;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	unsigned long temp;
	int error;
	int revents;

	error = cuse_client_get(&pcc);
	if (error != 0)
		goto pollnval;

	temp = 0;
	pcs = pcc->server;

	if (events & (POLLPRI | POLLIN | POLLRDNORM))
		temp |= CUSE_POLL_READ;

	if (events & (POLLOUT | POLLWRNORM))
		temp |= CUSE_POLL_WRITE;

	if (events & POLLHUP)
		temp |= CUSE_POLL_ERROR;

	pccmd = &pcc->cmds[CUSE_CMD_POLL];

	cuse_cmd_lock(pccmd);

	/* Need to selrecord() first to not loose any events. */
	if (temp != 0 && td != NULL)
		selrecord(td, &pcs->selinfo);

	cuse_server_lock(pcs);
	cuse_client_send_command_locked(pccmd,
	    0, temp, pcc->fflags, IO_NDELAY);

	error = cuse_client_receive_command_locked(pccmd, 0, 0);
	cuse_server_unlock(pcs);

	cuse_cmd_unlock(pccmd);

	if (error < 0) {
		goto pollnval;
	} else {
		revents = 0;
		if (error & CUSE_POLL_READ)
			revents |= (events & (POLLPRI | POLLIN | POLLRDNORM));
		if (error & CUSE_POLL_WRITE)
			revents |= (events & (POLLOUT | POLLWRNORM));
		if (error & CUSE_POLL_ERROR)
			revents |= (events & POLLHUP);
	}
	return (revents);

pollnval:
	/* XXX many clients don't understand POLLNVAL */
	return (events & (POLLHUP | POLLPRI | POLLIN |
	    POLLRDNORM | POLLOUT | POLLWRNORM));
}

static int
cuse_client_mmap_single(struct cdev *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	uint32_t page_nr = *offset / PAGE_SIZE;
	uint32_t alloc_nr = page_nr / CUSE_ALLOC_PAGES_MAX;
	struct cuse_memory *mem;
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	int error;

	error = cuse_client_get(&pcc);
	if (error != 0)
		return (error);

	pcs = pcc->server;

	cuse_server_lock(pcs);
	/* lookup memory structure */
	TAILQ_FOREACH(mem, &pcs->hmem, entry) {
		if (mem->alloc_nr == alloc_nr)
			break;
	}
	if (mem == NULL) {
		cuse_server_unlock(pcs);
		return (ENOMEM);
	}
	/* verify page offset */
	page_nr %= CUSE_ALLOC_PAGES_MAX;
	if (page_nr >= mem->page_count) {
		cuse_server_unlock(pcs);
		return (ENXIO);
	}
	/* verify mmap size */
	if ((size % PAGE_SIZE) != 0 || (size < PAGE_SIZE) ||
	    (size > ((mem->page_count - page_nr) * PAGE_SIZE))) {
		cuse_server_unlock(pcs);
		return (EINVAL);
	}
	vm_object_reference(mem->object);
	*object = mem->object;
	cuse_server_unlock(pcs);

	/* set new VM object offset to use */
	*offset = page_nr * PAGE_SIZE;

	/* success */
	return (0);
}

static void
cuse_client_kqfilter_read_detach(struct knote *kn)
{
	struct cuse_client *pcc;
	struct cuse_server *pcs;

	pcc = kn->kn_hook;
	pcs = pcc->server;

	cuse_server_lock(pcs);
	knlist_remove(&pcs->selinfo.si_note, kn, 1);
	cuse_server_unlock(pcs);
}

static void
cuse_client_kqfilter_write_detach(struct knote *kn)
{
	struct cuse_client *pcc;
	struct cuse_server *pcs;

	pcc = kn->kn_hook;
	pcs = pcc->server;

	cuse_server_lock(pcs);
	knlist_remove(&pcs->selinfo.si_note, kn, 1);
	cuse_server_unlock(pcs);
}

static int
cuse_client_kqfilter_read_event(struct knote *kn, long hint)
{
	struct cuse_client *pcc;

	pcc = kn->kn_hook;

	mtx_assert(&pcc->server->mtx, MA_OWNED);

	return ((pcc->cflags & CUSE_CLI_KNOTE_NEED_READ) ? 1 : 0);
}

static int
cuse_client_kqfilter_write_event(struct knote *kn, long hint)
{
	struct cuse_client *pcc;

	pcc = kn->kn_hook;

	mtx_assert(&pcc->server->mtx, MA_OWNED);

	return ((pcc->cflags & CUSE_CLI_KNOTE_NEED_WRITE) ? 1 : 0);
}

static int
cuse_client_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct cuse_client *pcc;
	struct cuse_server *pcs;
	int error;

	error = cuse_client_get(&pcc);
	if (error != 0)
		return (error);

	pcs = pcc->server;

	cuse_server_lock(pcs);
	switch (kn->kn_filter) {
	case EVFILT_READ:
		pcc->cflags |= CUSE_CLI_KNOTE_HAS_READ;
		kn->kn_hook = pcc;
		kn->kn_fop = &cuse_client_kqfilter_read_ops;
		knlist_add(&pcs->selinfo.si_note, kn, 1);
		break;
	case EVFILT_WRITE:
		pcc->cflags |= CUSE_CLI_KNOTE_HAS_WRITE;
		kn->kn_hook = pcc;
		kn->kn_fop = &cuse_client_kqfilter_write_ops;
		knlist_add(&pcs->selinfo.si_note, kn, 1);
		break;
	default:
		error = EINVAL;
		break;
	}
	cuse_server_unlock(pcs);

	if (error == 0)
		cuse_client_kqfilter_poll(dev, pcc);
	return (error);
}
