/*-
 * Copyright (c) 2006 Li, Xiao <intron@intron.ac>.  All rights reserved.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/eventhandler.h>
#include <sys/aio.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <vm/uma.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/linker.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/sysproto.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#define	LINUX_AIO_DEBUG

/*
 * Linux Kernel Implementation of Asynchronous I/O
 */

#ifdef	LINUX_AIO_DEBUG

/* Print arguments of syscall */
#define	DARGPRINTF(fmt, ...)	printf("linux(%ld): %s("fmt")\n",	\
	(long)td->td_proc->p_pid, __func__, __VA_ARGS__)
/* Print message in syscall function */
#define	DPRINTF(fmt, ...)	printf(LMSG("%s(): " fmt),		\
	__func__, __VA_ARGS__)
/* Print message in non-syscall function, the one more "P" means "private" */
#define	DPPRINTF(fmt, ...)	printf("linux(): %s(): " fmt "\n",	\
	__func__, __VA_ARGS__)

#else

#define	DARGPRINTF(fmt, ...)
#define	DPRINTF(fmt, ...)
#define	DPPRINTF(fmt, ...)

#endif

/*
 *                             DATA STRUCTURE HIERARCHY
 *
 *                   +--------------------+      +--------------------+
 * context_list ---> |       context      | ---> |       context      | ---> ...
 *             SLIST |(owned by a process)|      |(owned by a process)|
 *                   |                    |      |                    |
 *                   | ctx_req            |      | ctx_req            |
 *                   +----|---------------+      +----|---------------+
 *                        |  STAILQ                   |  STAILQ
 *                        v                           v
 *                    +------------+              +------------+
 *                    |   request  |              |   request  |
 *                    |            |              |            |
 *                    |.req_pbsd   |              |.req_pbsd   |
 *                    |.req_porig  |              |.req_porig  |
 *                    |.req_linux  |              |.req_linux  |
 *                    |            |              |            |
 *                    +------------+              +------------+
 *                        |                           |
 *                        v                           v
 *                    +------------+              +------------+
 *                    |   request  |              |   request  |
 *                    |            |              |            |
 *                    |.req_pbsd   |              |.req_pbsd   |
 *                    |.req_porig  |              |.req_porig  |
 *                    |.req_linux  |              |.req_linux  |
 *                    |            |              |            |
 *                    +------------+              +------------+
 *                        |                           |
 *                        v                           v
 *                       ...                         ...
 */

struct linux_aio_context;

struct linux_aio_request {
	struct aiocb        *req_pbsd;  /* Userland clone for FreeBSD */
	struct linux_iocb   *req_porig; /* Userland original control block */
	struct linux_iocb   req_linux;  /* Copy of original control block */
	STAILQ_ENTRY(linux_aio_request)	req_ctx_entry;
};

struct linux_aio_context {
	struct sx	ctx_sx;
	pid_t		ctx_pid;
	struct linux_aio_ring *ctx_pring;
	int		ctx_nreq_max; /* Maximum request number */
	int		ctx_nreq_cur; /* Current request number */
	STAILQ_HEAD(,linux_aio_request)	ctx_req;
	SLIST_ENTRY(linux_aio_context) ctx_list_entry;
};
static SLIST_HEAD(,linux_aio_context) linux_aio_context_list;

#define	LINUX_AIO_REQ_HOOK(pctx, preq)		{			\
	STAILQ_INSERT_TAIL(&((pctx)->ctx_req), (preq), req_ctx_entry);	\
	(pctx)->ctx_nreq_cur ++;					\
}

#define	LINUX_AIO_REQ_UNHOOK(pctx, preq) 	{			\
	STAILQ_REMOVE(&((pctx)->ctx_req), (preq), linux_aio_request,	\
			req_ctx_entry);					\
	(pctx)->ctx_nreq_cur --;					\
}

#define	LINUX_AIO_REQ_FOREACH(pctx, preq)				\
	STAILQ_FOREACH((preq), &((pctx)->ctx_req), req_ctx_entry)

#define	LINUX_AIO_REQ_FOREACH_SAFE(pctx, preq, ptmpreq)			\
	STAILQ_FOREACH_SAFE((preq), &((pctx)->ctx_req), req_ctx_entry,	\
			(ptmpreq))

#define	LINUX_AIO_CTX_LOCK(pctx)	sx_xlock(&((pctx)->ctx_sx))

#define	LINUX_AIO_CTX_UNLOCK(pctx)	sx_unlock(&((pctx)->ctx_sx))

#define	LINUX_AIO_CTX_HOOK(pctx)					\
	SLIST_INSERT_HEAD(&linux_aio_context_list, (pctx), ctx_list_entry)

#define	LINUX_AIO_CTX_UNHOOK(pctx)					\
	SLIST_REMOVE(&linux_aio_context_list, (pctx),			\
			linux_aio_context, ctx_list_entry)

#define	LINUX_AIO_CTX_FOREACH(pctx)					\
	SLIST_FOREACH((pctx), &linux_aio_context_list, ctx_list_entry)

#define	LINUX_AIO_CTX_FOREACH_SAFE(pctx, ptmpctx)			\
	SLIST_FOREACH_SAFE((pctx), &linux_aio_context_list,		\
			ctx_list_entry, (ptmpctx))

#define	LINUX_AIO_CTX_MATCH(pctx, ctxid, pid)				\
	((linux_aio_context_t)(pctx)->ctx_pring == (ctxid)		\
		&& (pctx)->ctx_pid == (pid))

static struct mtx linux_aio_context_list_mtx;

#define	LINUX_AIO_CTX_LIST_LOCK()	mtx_lock(&linux_aio_context_list_mtx)

#define	LINUX_AIO_CTX_LIST_UNLOCK()	mtx_unlock(&linux_aio_context_list_mtx)

/*
 * The following two macros are substantially identical to the two macros
 * AIO_(UN)LOCK in /sys/kern/vfs_aio.c. Thus, the mutex much be unlocked
 * before calling functions of FreeBSD native AIO module.
 *
 * XXX
 * I ASSUME the member "kaio_mtx" is the first element of "struct kaioinfo".
 */
#define	LINUX_AIO_LOCK(p)	{					\
	if ((p)->p_aioinfo == NULL)					\
		p_aio_init_aioinfo(p);					\
	mtx_lock((struct mtx *)((p)->p_aioinfo));			\
}

#define	LINUX_AIO_UNLOCK(p)	{					\
	if ((p)->p_aioinfo == NULL)					\
		p_aio_init_aioinfo(p);					\
	mtx_unlock((struct mtx *)((p)->p_aioinfo));			\
}

static uma_zone_t linux_aio_context_zone, linux_aio_request_zone;

static eventhandler_tag linux_aio_exit_tag;

/*
 * XXX
 * Calling external function/variable declared with "static" is DANGEROUS !!!
 * Compiler may use register to transfer calling arguments for optimization,
 * which is NOT a normal calling way and can cause kernel crash.
 */

#define	NATIVE_AIO_MODULE_NAME		"aio"
static struct mod_depend native_aio_module_depend = {1, 1, 1};
static linker_file_t native_aio_module_handle = NULL;

/* Mirror of sysctls in /sys/kern/vfs_aio.c */
#define	NATIVE_AIO_SYSCTL_CAPACITY_PROC	"vfs.aio.max_aio_queue_per_proc"
static int native_aio_capacity_proc;
#define	NATIVE_AIO_SYSCTL_CAPACITY_SYS	"vfs.aio.max_aio_queue"
static int native_aio_capacity_sys;

/* For declaration of aio_aqueue(), defined in /sys/kern/vfs_aio.c */
struct aioliojob;

/* Functions in /sys/kern/vfs_aio.c, XXX defined with "static" */
#define	GET_INTERNAL_FUNC_POINTER(s)	{				\
	* ((caddr_t *) & p_ ## s) = linker_file_lookup_symbol(		\
			native_aio_module_handle, #s, FALSE);		\
	if (p_ ## s == NULL)						\
		break;							\
}
static void (*p_aio_init_aioinfo) (struct proc *p);
static int (*p_aio_aqueue) (struct thread *td, struct aiocb *job,
			struct aioliojob *lio, int type, int osigev);

/* System calls in /sys/kern/vfs_aio.c */
#define	DEFINE_SYSCALL_POINTER_VARIABLE(s)				\
	static int (* p_ ## s) (struct thread *, struct s ## _args *)
#define	GET_SYSCALL_POINTER(s)		{				\
	* ((sy_call_t **) & p_ ## s) = sysent[SYS_ ## s].sy_call;	\
	if ((sy_call_t *) p_ ## s == (sy_call_t *)lkmressys)		\
		break;							\
}
DEFINE_SYSCALL_POINTER_VARIABLE(aio_return);
DEFINE_SYSCALL_POINTER_VARIABLE(aio_suspend);
DEFINE_SYSCALL_POINTER_VARIABLE(aio_cancel);
DEFINE_SYSCALL_POINTER_VARIABLE(aio_error);

static int user_mem_rw_verify(void *p, size_t s)
{
	char buf[256];
	size_t i;
	int nerr = 0;

	for (i = 0; i < s; i += sizeof(buf)) {
		/* Verify reading */
		nerr = copyin((char *)p+i, buf, MIN(sizeof(buf), s-i));
		if (nerr != 0)
			break;

		/* Verify writing */
		nerr = copyout(buf, (char *)p+i, MIN(sizeof(buf), s-i));
		if (nerr != 0)
			break;
	}

	return (nerr);
}

/* Allocate memory in user space */
static int user_malloc(struct thread *td, void **pp, size_t s)
{
	struct mmap_args mmaparg;
	int nerr;
	register_t r;

	r = td->td_retval[0];

	mmaparg.addr = NULL;
	mmaparg.len = s;
	mmaparg.prot = PROT_READ | PROT_WRITE;
	mmaparg.flags = MAP_PRIVATE | MAP_ANON;
	mmaparg.fd = -1;
	mmaparg.pad = 0;
	mmaparg.pos = 0;

	nerr = mmap(td, &mmaparg);

	if (nerr == 0) {
		*pp = (void *)td->td_retval[0];
		DPPRINTF("%lu bytes allocated at %p", (unsigned long)s, *pp);
	}

	td->td_retval[0] = r;

	return (nerr);
}

/* Free memory in user space */
static int user_free(struct thread *td, void *p, size_t s)
{
	struct munmap_args munmaparg;
	int nerr;
	register_t r;

	r = td->td_retval[0];

	munmaparg.addr = p;
	munmaparg.len = s;

	nerr = munmap(td, &munmaparg);

	td->td_retval[0] = r;
	DPPRINTF("%lu bytes at %p", (unsigned long)s, p);

	return (nerr);
}

#ifdef	LINUX_AIO_DEBUG

static void linux_aio_dump_freebsd_aiocb(struct aiocb *piocb, int isuserland)
{
	struct aiocb localcb, *pcb;
	int nerr = 0;

	if (isuserland) {
		nerr = copyin(piocb, &localcb, sizeof(localcb));
		pcb = &localcb;
	} else
		pcb = piocb;

	DPPRINTF("Dump struct aiocb (%p, %s): %s",
			piocb, (isuserland?"userland":"kernel"),
			(nerr?"Failure":""));
	if (!nerr) {
		DPPRINTF("aio_fildes: %d",
				pcb->aio_fildes);
		DPPRINTF("aio_offset: %lu",
				(unsigned long) pcb->aio_offset);
		DPPRINTF("aio_buf: %p",
				pcb->aio_buf);
		DPPRINTF("aio_nbytes: %lu",
				(unsigned long) pcb->aio_nbytes);
		DPPRINTF("aio_lio_opcode: %d",
				pcb->aio_lio_opcode);
		DPPRINTF("aio_reqprio: %d",
				pcb->aio_reqprio);
		DPPRINTF("aio_sigevent.sigev_notify: %d",
			       	pcb->aio_sigevent.sigev_notify);
		DPPRINTF("aio_sigevent.sigev_signo: %d",
			       	pcb->aio_sigevent.sigev_signo);
	}
}

#define	DUMP_FREEBSD_AIOCB(p, isu)    linux_aio_dump_freebsd_aiocb((p), (isu));

#define	DUMP_TIMESPEC(f, t ,a)						\
	DPRINTF("%s%ld second + %ld nanosecond%s",			\
			(f), (long)(t)->tv_sec, (long)(t)->tv_nsec, (a));

#else /* ! LINUX_AIO_DEBUG */

#define	DUMP_FREEBSD_AIOCB(p, isu)
#define	DUMP_TIMESPEC(f, t, a)

#endif /* LINUX_AIO_DEBUG */

static int iocb_reformat(struct linux_iocb *plnx, struct aiocb *pbsd)
{
	int nerr = 0;

	bzero(pbsd, sizeof(*pbsd));

	pbsd->aio_fildes = plnx->aio_fildes;  /* File descriptor */
	pbsd->aio_offset = plnx->aio_offset;  /* File offset for I/O */
	pbsd->aio_buf = (void *)(unsigned long) plnx->aio_buf; /*
								* User space
								* I/O buffer
								*/
	pbsd->aio_nbytes = plnx->aio_nbytes;  /* Number of bytes for I/O */
	switch (plnx->aio_lio_opcode) {       /* LIO opcode */
	case LINUX_IOCB_CMD_PREAD:
		pbsd->aio_lio_opcode = LIO_READ;
		break;
	case LINUX_IOCB_CMD_PWRITE:
		pbsd->aio_lio_opcode = LIO_WRITE;
		break;
	case LINUX_IOCB_CMD_FSYNC:
	case LINUX_IOCB_CMD_FDSYNC:
		pbsd->aio_lio_opcode = LIO_SYNC;
		break;
#if 0
	case LINUX_IOCB_CMD_PREADX:
		break;
	case LINUX_IOCB_CMD_POLL:
		break;
#endif
	case LINUX_IOCB_CMD_NOOP:
		pbsd->aio_lio_opcode = LIO_NOP;
		break;
	default:
		nerr = EINVAL;
		break;
	}
	if (nerr != 0) {
	        DPPRINTF("Unsupported aio_lio_opcode: %u",
	                        (unsigned)plnx->aio_lio_opcode);
	        return (nerr);
	}
	pbsd->aio_reqprio = plnx->aio_reqprio;        /* Request priority */
	pbsd->aio_sigevent.sigev_notify = SIGEV_NONE; /* No signal to deliver */
	pbsd->aio_sigevent.sigev_signo = 0;           /* No signal to deliver */

	return (nerr);
}

static int link_to_native_aio_module(struct thread *td)
{
	int nerr;

	if (native_aio_module_handle != NULL) {
	/* Linking has been done successfully. */
		return (0);
	}

	nerr = linker_reference_module(NATIVE_AIO_MODULE_NAME,
			&native_aio_module_depend, &native_aio_module_handle);
	if (nerr)
		return (nerr);

	do {
		nerr = EINVAL;

		/* Kernel internal functions */
		GET_INTERNAL_FUNC_POINTER(aio_init_aioinfo);
		GET_INTERNAL_FUNC_POINTER(aio_aqueue);

		/* System calls */
		GET_SYSCALL_POINTER(aio_return);
		GET_SYSCALL_POINTER(aio_suspend);
		GET_SYSCALL_POINTER(aio_cancel);
		GET_SYSCALL_POINTER(aio_error);

		nerr = 0;
	} while (0);

	if (nerr) {
		linker_release_module(NULL, NULL, native_aio_module_handle);
		native_aio_module_handle = NULL;
		
		printf(LMSG("Unable to link to the native module \""
				NATIVE_AIO_MODULE_NAME "\"."));
		
		return (nerr);
	}

	return (0);
}

#define	LINK_TO_NATIVE_AIO_MODULE()					\
	if (link_to_native_aio_module(td)) {				\
		printf(LMSG("Please load the module \""			\
			NATIVE_AIO_MODULE_NAME "\""			\
			"to provide FreeBSD "				\
			"native Asynchronous I/O support."));		\
		return (ENOSYS);						\
}

static int mirror_native_aio_sysctl(struct thread *td)
{
	int nerr = 0;
	size_t l;

	l = sizeof(native_aio_capacity_proc);
	nerr = kernel_sysctlbyname(td, NATIVE_AIO_SYSCTL_CAPACITY_PROC,
			&native_aio_capacity_proc, &l, NULL, 0,
			NULL ,0);
	if (nerr)
		return (nerr);

	l = sizeof(native_aio_capacity_sys);
	nerr = kernel_sysctlbyname(td, NATIVE_AIO_SYSCTL_CAPACITY_SYS,
			&native_aio_capacity_sys, &l, NULL, 0,
			NULL ,0);
	if (nerr)
		return (nerr);

	DPRINTF(NATIVE_AIO_SYSCTL_CAPACITY_PROC "=%d, "
			NATIVE_AIO_SYSCTL_CAPACITY_SYS "=%d",
			native_aio_capacity_proc,
			native_aio_capacity_sys);

	return (nerr);
}

/* Linux system call io_setup(2) */
int linux_io_setup(struct thread *td, struct linux_io_setup_args *args)
{
	struct proc *p;
	struct linux_aio_ring *pring, ring;
	struct linux_aio_context *pctx = NULL, *ptmpctx;
	linux_aio_context_t ctx_id;
	int nerr = 0, nr, nrall, nq, arg_nr_reqs;

	DARGPRINTF("%u, %p", args->nr_reqs, args->ctxp);
	LINK_TO_NATIVE_AIO_MODULE();
	nerr = mirror_native_aio_sysctl(td);
	if (nerr) {
		printf(LMSG("linux_io_setup(): Unable to query sysctls "
			       NATIVE_AIO_SYSCTL_CAPACITY_PROC
			       " and/or " NATIVE_AIO_SYSCTL_CAPACITY_SYS
			       " ."));
		return (nerr);
	}

	/* Signed integer is a little safer than unsigned */
	arg_nr_reqs = args->nr_reqs;
	if (arg_nr_reqs <= 0)
		return (EINVAL);

	if (arg_nr_reqs > native_aio_capacity_proc
			|| arg_nr_reqs > native_aio_capacity_sys) {
		printf(LMSG("linux_io_setup(): Please increase sysctls "
			       NATIVE_AIO_SYSCTL_CAPACITY_PROC
			       " and/or " NATIVE_AIO_SYSCTL_CAPACITY_SYS
			       " ."));
		return (ENOMEM);
	}

	nerr = user_mem_rw_verify(args->ctxp, sizeof(*(args->ctxp)));
	if (nerr != 0)
		return (nerr);

	copyin(args->ctxp, &ctx_id, sizeof(ctx_id));
	if (ctx_id != 0) /* "Not initialized", described by io_setup(2) */
		return (EINVAL);

	p = td->td_proc;

	/* Get a new "ring" */
	nerr = user_malloc(td, (void **)&pring, sizeof(*pring));
	if (nerr != 0)
		return (nerr);

	/* Get a new context */
	pctx = uma_zalloc(linux_aio_context_zone, M_WAITOK);

	LINUX_AIO_CTX_LIST_LOCK();

	/* Count request capacity of all contexts belonging to this process */
	nr = 0;
	nrall = 0;
	nq = 0;
	LINUX_AIO_CTX_FOREACH(ptmpctx) {
		if (ptmpctx->ctx_pid == p->p_pid) {
			nr += ptmpctx->ctx_nreq_max;
			nq ++;
		}
		nrall += ptmpctx->ctx_nreq_max;
	}
	DPRINTF("%d queues of %d requests totally allocated for this process, "
			"%d requests' total capacity for the whole system",
		nq, nr, nrall);

	/* Check whether there are enough resources for requested queue */
	if (arg_nr_reqs > native_aio_capacity_proc - nr
			|| arg_nr_reqs > native_aio_capacity_sys - nrall) {
		printf(LMSG("linux_io_setup(): "
			       "Please increase sysctls "
			       NATIVE_AIO_SYSCTL_CAPACITY_PROC
			       " and/or " NATIVE_AIO_SYSCTL_CAPACITY_SYS " ."
			       "Besides %d queues of %d requests totally "
			       "for this process, and %d requests' queues "
			       "totally for the whole system, "
			       "this Linux application needs one more "
			       "AIO queue of %d requests' capacity."),
			nq, nr, nrall, arg_nr_reqs);
		LINUX_AIO_CTX_LIST_UNLOCK();
		DPRINTF("Free context %p", pctx);
		uma_zfree(linux_aio_context_zone, pctx);
		user_free(td, pring, sizeof(*pring));
		return (ENOMEM);
	}

	/* Initialize the new context */
	sx_init(&(pctx->ctx_sx), "linux_aio_context");
	pctx->ctx_pid = p->p_pid;
	pctx->ctx_pring = pring;
	pctx->ctx_nreq_max = arg_nr_reqs;
	pctx->ctx_nreq_cur = 0;
	STAILQ_INIT(&(pctx->ctx_req));

	/* Hook the new context to global context list */
	LINUX_AIO_CTX_HOOK(pctx);

	LINUX_AIO_CTX_LIST_UNLOCK();

	/* Initialize the new "ring" */
	DPRINTF("initialize the \"ring\" %p", pring);
	bzero(&ring, sizeof(ring));
	ring.ring_id = 1;
	ring.ring_nr = arg_nr_reqs;
	ring.ring_head = 0;
	ring.ring_tail = 1;
	ring.ring_magic = LINUX_AIO_RING_MAGIC;
	ring.ring_compat_features = LINUX_AIO_RING_COMPAT_FEATURES;
	ring.ring_incompat_features = LINUX_AIO_RING_INCOMPAT_FEATURES;
	ring.ring_header_length = sizeof(ring);
	copyout(&ring, pring, sizeof(ring)); /* It has been hooked before */

	/* Substantial return value */
	ctx_id = (linux_aio_context_t)pctx->ctx_pring;
	copyout(&ctx_id, args->ctxp, sizeof(ctx_id));
	DPRINTF("returned context: %lx -> %p", (unsigned long)ctx_id, pctx);

	return (nerr);
}

/* Linux system call io_destroy(2) */
int linux_io_destroy(struct thread *td, struct linux_io_destroy_args *args)
{
	int nerr = 0;
	struct proc *p;
	struct linux_aio_context *pctx;
	struct linux_aio_request *preq, *ptmpreq;
	struct aio_cancel_args cancelargs;
	struct aio_return_args aioretargs;

	DARGPRINTF("%lx", (unsigned long)args->ctx);
	LINK_TO_NATIVE_AIO_MODULE();

	p = td->td_proc;

	/*
	 * Locking:
	 *
	 * LINUX_AIO_LOCK(p);   <----------------+
	 * ...                                   |
	 *     LINUX_AIO_CTX_LIST_LOCK();   <--+ |
	 *     ...                             | |
	 *     LINUX_AIO_CTX_LIST_UNLOCK(); <--+ |
	 * ...                                   |
	 * LINUX_AIO_CTX_LOCK(pctx);   <---------|---+
	 * LINUX_AIO_UNLOCK(p); <----------------+   |
	 * ...                                       |
	 * LINUX_AIO_CTX_UNLOCK(pctx); <-------------+
	 */

	LINUX_AIO_LOCK(p);

	/* Find the context in context list */
	LINUX_AIO_CTX_LIST_LOCK();
	LINUX_AIO_CTX_FOREACH(pctx) {
		if (LINUX_AIO_CTX_MATCH(pctx, args->ctx, p->p_pid))
			break;
	}
	LINUX_AIO_CTX_LIST_UNLOCK();

	/* Unable to find the context */
	if (pctx == NULL) {
		LINUX_AIO_UNLOCK(p);
		return (EINVAL);
	}

	DPRINTF("Found the context: %lx -> %p", (unsigned long)args->ctx, pctx);

	/* Unhook the context from context list */
	DPRINTF("Unhook context %p", pctx);
	LINUX_AIO_CTX_UNHOOK(pctx);

	LINUX_AIO_CTX_LOCK(pctx); /* XXX Interlaced, seamless */
	LINUX_AIO_UNLOCK(p);      /* XXX Interlaced, seamless */

	/* Real cleanup */
	LINUX_AIO_REQ_FOREACH_SAFE(pctx, preq, ptmpreq) {
		DPRINTF("Cancel request (Linux: %p, FreeBSD: %p)",
				preq->req_porig, preq->req_pbsd);

		/* Cancel FreeBSD native clone */
		cancelargs.fd = preq->req_linux.aio_fildes;
		cancelargs.aiocbp = preq->req_pbsd;
		p_aio_cancel(td, &cancelargs);
		DPRINTF("aio_cancel() returned %ld", (long)td->td_retval[0]);
		if (td->td_retval[0] == AIO_NOTCANCELED)
			printf(LMSG("linux_io_destroy(): Asynchronous IO "
					"request (Linux: %p, FreeBSD: %p) "
					"cannot be cancelled. "
					"***** Both User Space "
					"and Kernel Memory Leaked! *****"),
				preq->req_porig, preq->req_pbsd);

		LINUX_AIO_REQ_UNHOOK(pctx, preq);

		if (td->td_retval[0] == AIO_ALLDONE) {
			aioretargs.aiocbp = preq->req_pbsd;
			p_aio_return(td, &aioretargs);
			DPRINTF("aio_return(%p) returned %ld",
					aioretargs.aiocbp,
					(long)td->td_retval[0]);

			td->td_retval[0] = AIO_ALLDONE;
		}

		/* Free user space clone of the request */
		if (td->td_retval[0] != AIO_NOTCANCELED) /*
							 * XXX How to avoid
							 * memory leak here?
							 */
			user_free(td, preq->req_pbsd,
					sizeof(*(preq->req_pbsd)));

		/* Free kernel structure of the request */
		uma_zfree(linux_aio_request_zone, preq);

		td->td_retval[0] = 0;
	}

	LINUX_AIO_CTX_UNLOCK(pctx);

	sx_destroy(&(pctx->ctx_sx));

	/* Free the "ring" */
	DPRINTF("free the \"ring\" %p", pctx->ctx_pring);
	user_free(td, pctx->ctx_pring, sizeof(*pctx->ctx_pring));

	/* Free destroyed context */
	uma_zfree(linux_aio_context_zone, pctx);

	return (nerr);
}

/* Linux system call io_getevents(2) */
int linux_io_getevents(struct thread *td, struct linux_io_getevents_args *args)
{
	int i, j, nerr = 0;
	struct proc *p;
	struct l_timespec l_timeout;
	struct timespec timeout, *u_ptimeout, t1, t2;
	struct linux_aio_context *pctx;
	struct linux_aio_request *preq, *ptmpreq;
	struct linux_io_event evt;
	struct aio_return_args aioretargs;
	struct aio_error_args aioerrargs;
	register_t aio_ret, aio_err;
	struct aiocb ** u_aiocbp;
	struct aio_suspend_args aiosusargs;

	DARGPRINTF("%lx, %ld, %ld, %p, %p",
			(unsigned long) args->ctx_id,
			(long)args->min_nr, (long)args->nr,
			args->events, args->timeout);
	LINK_TO_NATIVE_AIO_MODULE();

	if (args->nr <= 0)
		return (EINVAL);

	if (args->min_nr < 0)
		return (EINVAL);

	nerr = user_mem_rw_verify(args->events,
			sizeof(*(args->events)) * args->nr);
	if (nerr != 0)
		return (nerr);

	if (args->timeout != NULL) {
		nerr = copyin(args->timeout, &l_timeout, sizeof(l_timeout));
		if (nerr != 0)
			return (nerr);
		timeout.tv_sec = l_timeout.tv_sec;
		timeout.tv_nsec = l_timeout.tv_nsec;
		DUMP_TIMESPEC("User specified timeout: ", &timeout, "");
	}

	p = td->td_proc;

	/*
	 * Locking:
	 *
	 * LINUX_AIO_LOCK(p);   <----------------+
	 * ...                                   |
	 *     LINUX_AIO_CTX_LIST_LOCK();   <--+ |
	 *     ...                             | |
	 *     LINUX_AIO_CTX_LIST_UNLOCK(); <--+ |
	 * ...                                   |
	 * LINUX_AIO_CTX_LOCK(pctx);   <---------|---+
	 * LINUX_AIO_UNLOCK(p); <----------------+   |
	 * ...                                       |
	 * LINUX_AIO_CTX_UNLOCK(pctx); <-------------+
	 */

	LINUX_AIO_LOCK(p);

	/* Find the context in context list */
	LINUX_AIO_CTX_LIST_LOCK();
	LINUX_AIO_CTX_FOREACH(pctx) {
		if (LINUX_AIO_CTX_MATCH(pctx, args->ctx_id, p->p_pid))
			break;
	}
	LINUX_AIO_CTX_LIST_UNLOCK();

	/* Unable to find the context */
	if (pctx == NULL) {
		LINUX_AIO_UNLOCK(p);
		return (EINVAL);
	}

	DPRINTF("Found the context: %lx -> %p", (unsigned long)args->ctx_id, pctx);

	LINUX_AIO_CTX_LOCK(pctx); /* XXX Interlaced, seamless */
	LINUX_AIO_UNLOCK(p);      /* XXX Interlaced, seamless */

	if (STAILQ_EMPTY(&(pctx->ctx_req))) {
		td->td_retval[0] = 0; /* No queued request */
		DPRINTF("No request in queue (context: %p) at all, "
				"return directly", pctx);
	} else { /* Deal with the request queue */
		i = 0; /*
			* This variable's value will be the return value
			* of linux_io_getevents() 
			*/

		nerr = user_malloc(td, (void **)&u_aiocbp,
				sizeof(*u_aiocbp) * pctx->ctx_nreq_max);
		if (nerr != 0)
			goto skip_substantial_0;

		nerr = user_malloc(td, (void **)&u_ptimeout,
				sizeof(*u_ptimeout));
		if (nerr != 0)
			goto skip_substantial_1;

		for (i = 0;i < args->nr;) {

			/* Collecting finished requests and waiting for queued requests */

			LINUX_AIO_REQ_FOREACH_SAFE(pctx, preq, ptmpreq) {
				
				/* Collect all finished requests */

				if (i >= args->nr) /* Full */
					break;

				aioerrargs.aiocbp = preq->req_pbsd;
				p_aio_error(td, &aioerrargs);
				aio_ret = td->td_retval[0];
				td->td_retval[0] = 0;

				DPRINTF("aio_error(%p) (Linux: %p) "
						"returned %ld%s",
					aioerrargs.aiocbp,
					preq->req_porig,
					(long)aio_ret,
					aio_ret == EINPROGRESS ?
						"(EINPROGRESS)" : "" );

				if (aio_ret == EINPROGRESS)
					continue;

				/* Done */
				LINUX_AIO_REQ_UNHOOK(pctx, preq);

				aioretargs.aiocbp = preq->req_pbsd;
				aio_err = p_aio_return(td, &aioretargs);
				aio_ret = td->td_retval[0];
				td->td_retval[0] = 0;

				DPRINTF("aio_return(%p) (Linux: %p) "
						"returned %ld, errno=%ld",
					aioretargs.aiocbp,
					preq->req_porig,
					(long)aio_ret,
					(long)aio_err);

				evt.data = preq->req_linux.aio_data;
				evt.obj = (uint64_t)(unsigned long)
					preq->req_porig;
				if (aio_ret >= 0) {
					/* Normal return (success) */
					evt.res = aio_ret;
				} else { /* Error code (failure) */
					/*
					 * Translate FreeBSD error code
					 * to Linux's
					 */
					evt.res =
					      p->p_sysent->sv_errtbl[aio_err];
				}
				DPRINTF("context %p (Linux: %p): "
						"io_event.res=%lld",
					preq->req_pbsd,
					preq->req_porig,
					(long long)evt.res);
				evt.res2 = 0;

				copyout(&evt, &(args->events[i]), sizeof(evt));

				uma_zfree(linux_aio_request_zone, preq);

				i ++;
			} /* End of collecting all finished requests */

			if (STAILQ_EMPTY(&(pctx->ctx_req))) {
				/* No request remained in this context */
				DPRINTF("returning(context %p): "
						"request queue is empty",
					pctx);
				break;
			}

			if (i >= args->nr) { /* Full */
				DPRINTF("returning(context %p): user space "
						"event array is full",
					pctx);
				break;
			}

			if (i >= args->min_nr) {
				/* Met the minimum requirement */
				DPRINTF("returning(context %p): "
						"met the minimum requirement",
					pctx);
				break;
			}

			if (args->timeout != NULL) {
				if (! timespecisset(&timeout)) { /* Timed out */
					DPRINTF("returning(context %p): "
							"no time remaining",
						pctx);
					break;
				}
			}

			if (args->timeout != NULL) {
				nanouptime(&t1); /* Time before aio_suspend() */
				DUMP_TIMESPEC("T1: ", &t1,
					" (uptime before calling aio_suspend())");
			}

			/* Prepare arguments for aio_suspend() */
			j = 0;
			LINUX_AIO_REQ_FOREACH(pctx, preq) {
				copyout(&(preq->req_pbsd), &(u_aiocbp[j]),
					sizeof(preq->req_pbsd));
				j++;
			}
			MPASS(j == pctx->ctx_nreq_cur);
			aiosusargs.aiocbp = u_aiocbp;
			aiosusargs.nent = j;

			if (args->timeout != NULL) {
				copyout(&timeout, u_ptimeout, sizeof(timeout));
				aiosusargs.timeout = u_ptimeout;
				DUMP_TIMESPEC("Time remained: ", &timeout, "");
			} else {
				aiosusargs.timeout = NULL;
			}

			aio_err = p_aio_suspend(td, &aiosusargs);
			DPRINTF("aio_suspend(%p, %d, %p) returned %ld",
					aiosusargs.aiocbp, aiosusargs.nent,
					aiosusargs.timeout, (long)aio_err);

			if (args->timeout != NULL) {
				nanouptime(&t2); /* Time after aio_suspend() */
				DUMP_TIMESPEC("T2: ", &t2,
					" (uptime after calling aio_suspend())");
				timespecsub(&t2, &t1); /*
							* Time spent by
							* aio_suspend()
							*/
				DUMP_TIMESPEC("T_delta: ", &t2,
					" (time spent by calling aio_suspend())");
				if (timespeccmp(&t2, &timeout, >=)) {
					timespecclear(&timeout); /* Timed out */
				} else {
					timespecsub(&timeout, &t2);
					/* Time remaining */
				}
				DUMP_TIMESPEC("Time remained: ", &timeout, "");
			}

			if (aio_err == EAGAIN) { /* Timed out */
				DPRINTF("returning(context %p): "
						"timed out after calling aio_suspend()",
					pctx);
				break;
			}
		} /* 
		   * End of collecting finished requests
		   * and waiting for queued requests
		   */

		l_timeout.tv_sec = timeout.tv_sec;
		l_timeout.tv_nsec = timeout.tv_nsec;
		copyout(&l_timeout, args->timeout, sizeof(l_timeout));
		/* No matter whether successfully or not */

		nerr = user_free(td, u_ptimeout, sizeof(*u_ptimeout));
skip_substantial_1:
		nerr = user_free(td, u_aiocbp,
				sizeof(*u_aiocbp) * pctx->ctx_nreq_max);
skip_substantial_0:
		td->td_retval[0] = i;
		/* user_free() resets td->td_retval[0] to 0 */
		DPRINTF("%d requests are unhooked from the context %p", i, pctx);
	} /* End of dealing with request queue */

	LINUX_AIO_CTX_UNLOCK(pctx);

	return (nerr);
}

/* Linux system call io_submit(2) */
int linux_io_submit(struct thread *td, struct linux_io_submit_args *args)
{
	int i, nerr = 0;
	struct proc *p;
	struct linux_aio_context *pctx;
	struct linux_aio_request req, *preq;
	struct linux_iocb *porig;
	struct aiocb iocb, *piocb;

	DARGPRINTF("%lx, %ld, %p", (unsigned long)args->ctx_id,
			(long)args->nr, args->iocbpp);
	LINK_TO_NATIVE_AIO_MODULE();

	if (args->nr <= 0)
		return (EINVAL);

	p = td->td_proc;

	/*
	 * Locking:
	 *
	 * LINUX_AIO_LOCK(p);   <----------------+
	 * ...                                   |
	 *     LINUX_AIO_CTX_LIST_LOCK();   <--+ |
	 *     ...                             | |
	 *     LINUX_AIO_CTX_LIST_UNLOCK(); <--+ |
	 * ...                                   |
	 * LINUX_AIO_CTX_LOCK(pctx);   <---------|---+
	 * LINUX_AIO_UNLOCK(p); <----------------+   |
	 * ...                                       |
	 * LINUX_AIO_CTX_UNLOCK(pctx); <-------------+
	 */

	LINUX_AIO_LOCK(p);

	/* Find the context in context list */
	LINUX_AIO_CTX_LIST_LOCK();
	LINUX_AIO_CTX_FOREACH(pctx) {
		if (LINUX_AIO_CTX_MATCH(pctx, args->ctx_id, p->p_pid))
			break;
	}
	LINUX_AIO_CTX_LIST_UNLOCK();

	/* Unable to find the context */
	if (pctx == NULL) {
		LINUX_AIO_UNLOCK(p);
		return (EINVAL);
	}

	DPRINTF("Found the context: %lx -> %p", (unsigned long)args->ctx_id, pctx);

	LINUX_AIO_CTX_LOCK(pctx); /* XXX Interlaced, seamless */
	LINUX_AIO_UNLOCK(p);      /* XXX Interlaced, seamless */

	for (i = 0; pctx->ctx_nreq_cur < pctx->ctx_nreq_max && i < args->nr;
			i++) {
		/* Get user space Linux control block  */
		nerr = copyin(&(args->iocbpp[i]), &porig, sizeof(porig));
		if (nerr != 0)
			break;
		nerr = copyin(porig, &(req.req_linux), sizeof(req.req_linux));
		if (nerr != 0)
			break;

		/* Create user space FreeBSD control block clone */
		nerr = iocb_reformat(&(req.req_linux), &iocb);
		if (nerr != 0)
			break;
		nerr = user_malloc(td, (void **)&piocb, sizeof(*piocb));
		if (nerr != 0)
			break;
		nerr = copyout(&iocb, piocb, sizeof(iocb));
		if (nerr != 0)
			break;
		DUMP_FREEBSD_AIOCB(piocb, 1);

		/* Submit user space control block */
		nerr = p_aio_aqueue(td, piocb, NULL, iocb.aio_lio_opcode, 0);
		if (nerr != 0) {
			user_free(td, piocb, sizeof(*piocb));
			break;
		}

		req.req_porig = porig;
		req.req_pbsd = piocb;

		/* Hook request to the context */
		preq = uma_zalloc(linux_aio_request_zone, M_WAITOK);
		memcpy(preq, &req, sizeof(req));
		DPRINTF("Linux IOCB %p (aio_lio_opcode=%u, aio_fildes=%u), "
				"FreeBSD IOCB %p",
			preq->req_porig,
			(unsigned)preq->req_linux.aio_lio_opcode,
			(unsigned)preq->req_linux.aio_fildes,
			preq->req_pbsd);
		LINUX_AIO_REQ_HOOK(pctx, preq);
	}

	LINUX_AIO_CTX_UNLOCK(pctx);

	if (i > 0) {
		td->td_retval[0] = i;
		nerr = 0;
	}

	if (i == 0 && nerr == 0)
		nerr = EAGAIN; /* No request is successfully submitted */

	return (nerr);
}

/* Linux system call io_cancel(2) */
int linux_io_cancel(struct thread *td, struct linux_io_cancel_args *args)
{
	int nerr = 0;
	struct proc *p;
	struct linux_iocb lcb;
	struct linux_aio_context *pctx;
	struct linux_aio_request *preq;
	struct linux_io_event evt;
	struct aio_cancel_args aiocnclargs;

	DARGPRINTF("%lx, %p, %p", (unsigned long)args->ctx_id,
			args->iocb, args->result);
	LINK_TO_NATIVE_AIO_MODULE();

	nerr = copyin(args->iocb, &lcb, sizeof(lcb));
	if (nerr != 0)
		return (nerr);

	nerr = user_mem_rw_verify(args->result, sizeof(*(args->result)));
	if (nerr != 0)
		return (nerr);

	p = td->td_proc;

	/*
	 * Locking:
	 *
	 * LINUX_AIO_LOCK(p);   <----------------+
	 * ...                                   |
	 *     LINUX_AIO_CTX_LIST_LOCK();   <--+ |
	 *     ...                             | |
	 *     LINUX_AIO_CTX_LIST_UNLOCK(); <--+ |
	 * ...                                   |
	 * LINUX_AIO_CTX_LOCK(pctx);   <---------|---+
	 * LINUX_AIO_UNLOCK(p); <----------------+   |
	 * ...                                       |
	 * LINUX_AIO_CTX_UNLOCK(pctx); <-------------+
	 */

	LINUX_AIO_LOCK(p);

	/* Find the context in context list */
	LINUX_AIO_CTX_LIST_LOCK();
	LINUX_AIO_CTX_FOREACH(pctx) {
		if (LINUX_AIO_CTX_MATCH(pctx, args->ctx_id, p->p_pid))
			break;
	}
	LINUX_AIO_CTX_LIST_UNLOCK();

	/* Unable to find the context */
	if (pctx == NULL) {
		LINUX_AIO_UNLOCK(p);
		return (EINVAL);
	}

	DPRINTF("Found the context: %lx -> %p", (unsigned long)args->ctx_id, pctx);

	LINUX_AIO_CTX_LOCK(pctx); /* XXX Interlaced, seamless */
	LINUX_AIO_UNLOCK(p);      /* XXX Interlaced, seamless */

	LINUX_AIO_REQ_FOREACH(pctx, preq) {
		if (preq->req_porig == args->iocb
				&& preq->req_linux.aio_key == lcb.aio_key)
			break;
	}

	if (preq == NULL) {
		DPRINTF("Unable to find IO control block %p", args->iocb);
		nerr = EINVAL;
	} else { /* Found the request in context */
		DPRINTF("Cancel request (Linux: %p, FreeBSD: %p)",
				preq->req_porig, preq->req_pbsd);

		/* Cancel FreeBSD native clone */
		aiocnclargs.fd = preq->req_linux.aio_fildes;
		aiocnclargs.aiocbp = preq->req_pbsd;
		p_aio_cancel(td, &aiocnclargs);
		DPRINTF("aio_cancel() returned %ld", (long)td->td_retval[0]);

		if (td->td_retval[0] == AIO_CANCELED) {
			/* Cancellation succeeded */
			LINUX_AIO_REQ_UNHOOK(pctx, preq);

			evt.data = preq->req_linux.aio_data;
			evt.obj = (uint64_t)(unsigned long) preq->req_porig;
			evt.res = p->p_sysent->sv_errtbl[ECANCELED];
			evt.res2 = 0;

			/* Fill in user space structure linux_io_event */
			copyout(&evt, args->result, sizeof(evt));

			/* Free user space clone of the request */
			user_free(td, preq->req_pbsd,
					sizeof(*(preq->req_pbsd)));

			/* Free kernel structure of the request */
			uma_zfree(linux_aio_request_zone, preq);
		} else if (td->td_retval[0] == AIO_ALLDONE) {
			nerr = EINVAL; /*
					* This value of Linux 2.6.15
					* is really confusing !!!
					*/
		} else { /* AIO_NOTCANCELED */
			nerr = EAGAIN;
		}

		td->td_retval[0] = 0;
	}

	LINUX_AIO_CTX_UNLOCK(pctx);

	return (nerr);
}

static void linux_aio_proc_rundown(void *arg, struct proc *p)
{
	struct linux_aio_context *pctx, *ptmpctx;
	struct linux_aio_request *preq, *ptmpreq;

	/*
	 * FreeBSD module "aio" can do more essential native cleanup
	 * (i.e. cancelling all queued requests) itself.
	 */

	LINUX_AIO_CTX_LIST_LOCK();

	LINUX_AIO_CTX_FOREACH_SAFE(pctx, ptmpctx) {
		if (pctx->ctx_pid == p->p_pid) {
			LINUX_AIO_REQ_FOREACH_SAFE(pctx, preq, ptmpreq) {
				DPPRINTF("Free request %p from context %p "
						"(ring: %p)",
					preq, pctx, pctx->ctx_pring);
				LINUX_AIO_REQ_UNHOOK(pctx, preq);
				uma_zfree(linux_aio_request_zone, preq);
			}

			DPPRINTF("Free context %p (ring: %p)",
					pctx, pctx->ctx_pring);

			/* Unhook it from context list */
			LINUX_AIO_CTX_UNHOOK(pctx);

			/* Free it really */
			sx_destroy(&(pctx->ctx_sx));
			uma_zfree(linux_aio_context_zone, pctx);

			DPPRINTF("The remaining context list is %s",
				(SLIST_EMPTY(&linux_aio_context_list) ?
				 	"empty":"not empty"));
		}
	}

	LINUX_AIO_CTX_LIST_UNLOCK();
}

/*
 * Module constructor/destructor
 */
static int
linux_aio_modload(struct module *module, int cmd, void *arg)
{
	int nerr = 0;

	switch (cmd) {
	case MOD_LOAD:
		linux_aio_context_zone = uma_zcreate("LINUXAIOCTX",
				sizeof(struct linux_aio_context),
				NULL, NULL, NULL, NULL,
				UMA_ALIGN_PTR, 0);
		linux_aio_request_zone = uma_zcreate("LINUXAIOREQ",
				sizeof(struct linux_aio_request),
				NULL, NULL, NULL, NULL,
				UMA_ALIGN_PTR, 0);
		mtx_init(&linux_aio_context_list_mtx,
				"linux_aio_context_list", NULL, MTX_DEF);
		SLIST_INIT(&linux_aio_context_list);
		linux_aio_exit_tag = EVENTHANDLER_REGISTER(process_exit,
				linux_aio_proc_rundown,
				NULL, EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		LINUX_AIO_CTX_LIST_LOCK();
		if (!SLIST_EMPTY(&linux_aio_context_list)) {
			nerr = EBUSY;
			LINUX_AIO_CTX_LIST_UNLOCK();
			break;
		}
		EVENTHANDLER_DEREGISTER(process_exit, linux_aio_exit_tag);
		LINUX_AIO_CTX_LIST_UNLOCK();
		mtx_destroy(&linux_aio_context_list_mtx);
		uma_zdestroy(linux_aio_request_zone);
		uma_zdestroy(linux_aio_context_zone);
		if (native_aio_module_handle != NULL) {
			/*
			 * linker_release_module() cannot be used here.
			 * It tries to hold "kld_sx", conflicting against
			 * module_unload().
			 */
			linker_file_unload(native_aio_module_handle,
				LINKER_UNLOAD_NORMAL);
		}
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		nerr = EINVAL;
		break;
	}
	return (nerr);
}

static moduledata_t linux_aio_mod = {
	"linuxaio",
	&linux_aio_modload,
	NULL
};

DECLARE_MODULE(linuxaio, linux_aio_mod, SI_SUB_VFS, SI_ORDER_ANY);
