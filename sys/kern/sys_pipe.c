/*
 * Copyright (c) 1996 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $Id: sys_pipe.c,v 1.41 1998/03/28 10:33:07 bde Exp $
 */

/*
 * This file contains a high-performance replacement for the socket-based
 * pipes scheme originally used in FreeBSD/4.4Lite.  It does not support
 * all features of sockets, but does do everything that pipes normally
 * do.
 */

/*
 * This code has two modes of operation, a small write mode and a large
 * write mode.  The small write mode acts like conventional pipes with
 * a kernel buffer.  If the buffer is less than PIPE_MINDIRECT, then the
 * "normal" pipe buffering is done.  If the buffer is between PIPE_MINDIRECT
 * and PIPE_SIZE in size, it is fully mapped and wired into the kernel, and
 * the receiving process can copy it directly from the pages in the sending
 * process.
 *
 * If the sending process receives a signal, it is possible that it will
 * go away, and certainly its address space can change, because control
 * is returned back to the user-mode side.  In that case, the pipe code
 * arranges to copy the buffer supplied by the user process, to a pageable
 * kernel buffer, and the receiving process will grab the data from the
 * pageable kernel buffer.  Since signals don't happen all that often,
 * the copy operation is normally eliminated.
 *
 * The constant PIPE_MINDIRECT is chosen to make sure that buffering will
 * happen for small transfers so that the system will not spend all of
 * its time context switching.  PIPE_SIZE is constrained by the
 * amount of kernel virtual memory.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/ttycom.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/signalvar.h>
#include <sys/sysproto.h>
#include <sys/pipe.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_zone.h>

/*
 * Use this define if you want to disable *fancy* VM things.  Expect an
 * approx 30% decrease in transfer rate.  This could be useful for
 * NetBSD or OpenBSD.
 */
/* #define PIPE_NODIRECT */

/*
 * interfaces to the outside world
 */
static int pipe_read __P((struct file *fp, struct uio *uio, 
		struct ucred *cred));
static int pipe_write __P((struct file *fp, struct uio *uio, 
		struct ucred *cred));
static int pipe_close __P((struct file *fp, struct proc *p));
static int pipe_poll __P((struct file *fp, int events, struct ucred *cred,
		struct proc *p));
static int pipe_ioctl __P((struct file *fp, u_long cmd, caddr_t data, struct proc *p));

static struct fileops pipeops =
    { pipe_read, pipe_write, pipe_ioctl, pipe_poll, pipe_close };

/*
 * Default pipe buffer size(s), this can be kind-of large now because pipe
 * space is pageable.  The pipe code will try to maintain locality of
 * reference for performance reasons, so small amounts of outstanding I/O
 * will not wipe the cache.
 */
#define MINPIPESIZE (PIPE_SIZE/3)
#define MAXPIPESIZE (2*PIPE_SIZE/3)

/*
 * Maximum amount of kva for pipes -- this is kind-of a soft limit, but
 * is there so that on large systems, we don't exhaust it.
 */
#define MAXPIPEKVA (8*1024*1024)

/*
 * Limit for direct transfers, we cannot, of course limit
 * the amount of kva for pipes in general though.
 */
#define LIMITPIPEKVA (16*1024*1024)

/*
 * Limit the number of "big" pipes
 */
#define LIMITBIGPIPES	32
static int nbigpipe;

static int amountpipekva;

static void pipeclose __P((struct pipe *cpipe));
static void pipeinit __P((struct pipe *cpipe));
static __inline int pipelock __P((struct pipe *cpipe, int catch));
static __inline void pipeunlock __P((struct pipe *cpipe));
static __inline void pipeselwakeup __P((struct pipe *cpipe));
#ifndef PIPE_NODIRECT
static int pipe_build_write_buffer __P((struct pipe *wpipe, struct uio *uio));
static void pipe_destroy_write_buffer __P((struct pipe *wpipe));
static int pipe_direct_write __P((struct pipe *wpipe, struct uio *uio));
static void pipe_clone_write_buffer __P((struct pipe *wpipe));
#endif
static void pipespace __P((struct pipe *cpipe));

static vm_zone_t pipe_zone;

/*
 * The pipe system call for the DTYPE_PIPE type of pipes
 */

/* ARGSUSED */
int
pipe(p, uap)
	struct proc *p;
	struct pipe_args /* {
		int	dummy;
	} */ *uap;
{
	register struct filedesc *fdp = p->p_fd;
	struct file *rf, *wf;
	struct pipe *rpipe, *wpipe;
	int fd, error;

	if (pipe_zone == NULL)
		pipe_zone = zinit("PIPE", sizeof (struct pipe), 0, 0, 4);

	rpipe = zalloc( pipe_zone);
	pipeinit(rpipe);
	rpipe->pipe_state |= PIPE_DIRECTOK;
	wpipe = zalloc( pipe_zone);
	pipeinit(wpipe);
	wpipe->pipe_state |= PIPE_DIRECTOK;

	error = falloc(p, &rf, &fd);
	if (error)
		goto free2;
	p->p_retval[0] = fd;
	rf->f_flag = FREAD | FWRITE;
	rf->f_type = DTYPE_PIPE;
	rf->f_ops = &pipeops;
	rf->f_data = (caddr_t)rpipe;
	error = falloc(p, &wf, &fd);
	if (error)
		goto free3;
	wf->f_flag = FREAD | FWRITE;
	wf->f_type = DTYPE_PIPE;
	wf->f_ops = &pipeops;
	wf->f_data = (caddr_t)wpipe;
	p->p_retval[1] = fd;

	rpipe->pipe_peer = wpipe;
	wpipe->pipe_peer = rpipe;

	return (0);
free3:
	ffree(rf);
	fdp->fd_ofiles[p->p_retval[0]] = 0;
free2:
	(void)pipeclose(wpipe);
	(void)pipeclose(rpipe);
	return (error);
}

/*
 * Allocate kva for pipe circular buffer, the space is pageable
 */
static void
pipespace(cpipe)
	struct pipe *cpipe;
{
	int npages, error;

	npages = round_page(cpipe->pipe_buffer.size)/PAGE_SIZE;
	/*
	 * Create an object, I don't like the idea of paging to/from
	 * kernel_object.
	 * XXX -- minor change needed here for NetBSD/OpenBSD VM systems.
	 */
	cpipe->pipe_buffer.object = vm_object_allocate(OBJT_DEFAULT, npages);
	cpipe->pipe_buffer.buffer = (caddr_t) vm_map_min(kernel_map);

	/*
	 * Insert the object into the kernel map, and allocate kva for it.
	 * The map entry is, by default, pageable.
	 * XXX -- minor change needed here for NetBSD/OpenBSD VM systems.
	 */
	error = vm_map_find(kernel_map, cpipe->pipe_buffer.object, 0,
		(vm_offset_t *) &cpipe->pipe_buffer.buffer, 
		cpipe->pipe_buffer.size, 1,
		VM_PROT_ALL, VM_PROT_ALL, 0);

	if (error != KERN_SUCCESS)
		panic("pipeinit: cannot allocate pipe -- out of kvm -- code = %d", error);
	amountpipekva += cpipe->pipe_buffer.size;
}

/*
 * initialize and allocate VM and memory for pipe
 */
static void
pipeinit(cpipe)
	struct pipe *cpipe;
{

	cpipe->pipe_buffer.in = 0;
	cpipe->pipe_buffer.out = 0;
	cpipe->pipe_buffer.cnt = 0;
	cpipe->pipe_buffer.size = PIPE_SIZE;

	/* Buffer kva gets dynamically allocated */
	cpipe->pipe_buffer.buffer = NULL;
	/* cpipe->pipe_buffer.object = invalid */

	cpipe->pipe_state = 0;
	cpipe->pipe_peer = NULL;
	cpipe->pipe_busy = 0;
	getnanotime(&cpipe->pipe_ctime);
	cpipe->pipe_atime = cpipe->pipe_ctime;
	cpipe->pipe_mtime = cpipe->pipe_ctime;
	bzero(&cpipe->pipe_sel, sizeof cpipe->pipe_sel);
	cpipe->pipe_pgid = NO_PID;

#ifndef PIPE_NODIRECT
	/*
	 * pipe data structure initializations to support direct pipe I/O
	 */
	cpipe->pipe_map.cnt = 0;
	cpipe->pipe_map.kva = 0;
	cpipe->pipe_map.pos = 0;
	cpipe->pipe_map.npages = 0;
	/* cpipe->pipe_map.ms[] = invalid */
#endif
}


/*
 * lock a pipe for I/O, blocking other access
 */
static __inline int
pipelock(cpipe, catch)
	struct pipe *cpipe;
	int catch;
{
	int error;
	while (cpipe->pipe_state & PIPE_LOCK) {
		cpipe->pipe_state |= PIPE_LWANT;
		if (error = tsleep( cpipe,
			catch?(PRIBIO|PCATCH):PRIBIO, "pipelk", 0)) {
			return error;
		}
	}
	cpipe->pipe_state |= PIPE_LOCK;
	return 0;
}

/*
 * unlock a pipe I/O lock
 */
static __inline void
pipeunlock(cpipe)
	struct pipe *cpipe;
{
	cpipe->pipe_state &= ~PIPE_LOCK;
	if (cpipe->pipe_state & PIPE_LWANT) {
		cpipe->pipe_state &= ~PIPE_LWANT;
		wakeup(cpipe);
	}
}

static __inline void
pipeselwakeup(cpipe)
	struct pipe *cpipe;
{
	struct proc *p;

	if (cpipe->pipe_state & PIPE_SEL) {
		cpipe->pipe_state &= ~PIPE_SEL;
		selwakeup(&cpipe->pipe_sel);
	}
	if (cpipe->pipe_state & PIPE_ASYNC) {
		if (cpipe->pipe_pgid < 0)
			gsignal(-cpipe->pipe_pgid, SIGIO);
		else if ((p = pfind(cpipe->pipe_pgid)) != NULL)
			psignal(p, SIGIO);
	}
}

/* ARGSUSED */
static int
pipe_read(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{

	struct pipe *rpipe = (struct pipe *) fp->f_data;
	int error = 0;
	int nread = 0;
	u_int size;

	++rpipe->pipe_busy;
	while (uio->uio_resid) {
		/*
		 * normal pipe buffer receive
		 */
		if (rpipe->pipe_buffer.cnt > 0) {
			size = rpipe->pipe_buffer.size - rpipe->pipe_buffer.out;
			if (size > rpipe->pipe_buffer.cnt)
				size = rpipe->pipe_buffer.cnt;
			if (size > (u_int) uio->uio_resid)
				size = (u_int) uio->uio_resid;
			if ((error = pipelock(rpipe,1)) == 0) {
				error = uiomove( &rpipe->pipe_buffer.buffer[rpipe->pipe_buffer.out], 
					size, uio);
				pipeunlock(rpipe);
			}
			if (error) {
				break;
			}
			rpipe->pipe_buffer.out += size;
			if (rpipe->pipe_buffer.out >= rpipe->pipe_buffer.size)
				rpipe->pipe_buffer.out = 0;

			rpipe->pipe_buffer.cnt -= size;
			nread += size;
#ifndef PIPE_NODIRECT
		/*
		 * Direct copy, bypassing a kernel buffer.
		 */
		} else if ((size = rpipe->pipe_map.cnt) &&
			(rpipe->pipe_state & PIPE_DIRECTW)) {
			caddr_t va;
			if (size > (u_int) uio->uio_resid)
				size = (u_int) uio->uio_resid;
			if ((error = pipelock(rpipe,1)) == 0) {
				va = (caddr_t) rpipe->pipe_map.kva + rpipe->pipe_map.pos;
				error = uiomove(va, size, uio);
				pipeunlock(rpipe);
			}
			if (error)
				break;
			nread += size;
			rpipe->pipe_map.pos += size;
			rpipe->pipe_map.cnt -= size;
			if (rpipe->pipe_map.cnt == 0) {
				rpipe->pipe_state &= ~PIPE_DIRECTW;
				wakeup(rpipe);
			}
#endif
		} else {
			/*
			 * detect EOF condition
			 */
			if (rpipe->pipe_state & PIPE_EOF) {
				/* XXX error = ? */
				break;
			}
			/*
			 * If the "write-side" has been blocked, wake it up now.
			 */
			if (rpipe->pipe_state & PIPE_WANTW) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}
			if (nread > 0)
				break;

			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
				break;
			}

			/*
			 * If there is no more to read in the pipe, reset
			 * its pointers to the beginning.  This improves
			 * cache hit stats.
			 */
		
			if ((error = pipelock(rpipe,1)) == 0) {
				if (rpipe->pipe_buffer.cnt == 0) {
					rpipe->pipe_buffer.in = 0;
					rpipe->pipe_buffer.out = 0;
				}
				pipeunlock(rpipe);
			} else {
				break;
			}

			if (rpipe->pipe_state & PIPE_WANTW) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}

			rpipe->pipe_state |= PIPE_WANTR;
			if (error = tsleep(rpipe, PRIBIO|PCATCH, "piperd", 0)) {
				break;
			}
		}
	}

	if (error == 0)
		getnanotime(&rpipe->pipe_atime);

	--rpipe->pipe_busy;
	if ((rpipe->pipe_busy == 0) && (rpipe->pipe_state & PIPE_WANT)) {
		rpipe->pipe_state &= ~(PIPE_WANT|PIPE_WANTW);
		wakeup(rpipe);
	} else if (rpipe->pipe_buffer.cnt < MINPIPESIZE) {
		/*
		 * If there is no more to read in the pipe, reset
		 * its pointers to the beginning.  This improves
		 * cache hit stats.
		 */
		if (rpipe->pipe_buffer.cnt == 0) {
			if ((error == 0) && (error = pipelock(rpipe,1)) == 0) {
				rpipe->pipe_buffer.in = 0;
				rpipe->pipe_buffer.out = 0;
				pipeunlock(rpipe);
			}
		}

		/*
		 * If the "write-side" has been blocked, wake it up now.
		 */
		if (rpipe->pipe_state & PIPE_WANTW) {
			rpipe->pipe_state &= ~PIPE_WANTW;
			wakeup(rpipe);
		}
	}

	if ((rpipe->pipe_buffer.size - rpipe->pipe_buffer.cnt) >= PIPE_BUF)
		pipeselwakeup(rpipe);

	return error;
}

#ifndef PIPE_NODIRECT
/*
 * Map the sending processes' buffer into kernel space and wire it.
 * This is similar to a physical write operation.
 */
static int
pipe_build_write_buffer(wpipe, uio)
	struct pipe *wpipe;
	struct uio *uio;
{
	u_int size;
	int i;
	vm_offset_t addr, endaddr, paddr;

	size = (u_int) uio->uio_iov->iov_len;
	if (size > wpipe->pipe_buffer.size)
		size = wpipe->pipe_buffer.size;

	endaddr = round_page(uio->uio_iov->iov_base + size);
	for(i = 0, addr = trunc_page(uio->uio_iov->iov_base);
		addr < endaddr;
		addr += PAGE_SIZE, i+=1) {

		vm_page_t m;

		vm_fault_quick( (caddr_t) addr, VM_PROT_READ);
		paddr = pmap_kextract(addr);
		if (!paddr) {
			int j;
			for(j=0;j<i;j++)
				vm_page_unwire(wpipe->pipe_map.ms[j]);
			return EFAULT;
		}

		m = PHYS_TO_VM_PAGE(paddr);
		vm_page_wire(m);
		wpipe->pipe_map.ms[i] = m;
	}

/*
 * set up the control block
 */
	wpipe->pipe_map.npages = i;
	wpipe->pipe_map.pos = ((vm_offset_t) uio->uio_iov->iov_base) & PAGE_MASK;
	wpipe->pipe_map.cnt = size;

/*
 * and map the buffer
 */
	if (wpipe->pipe_map.kva == 0) {
		/*
		 * We need to allocate space for an extra page because the
		 * address range might (will) span pages at times.
		 */
		wpipe->pipe_map.kva = kmem_alloc_pageable(kernel_map,
			wpipe->pipe_buffer.size + PAGE_SIZE);
		amountpipekva += wpipe->pipe_buffer.size + PAGE_SIZE;
	}
	pmap_qenter(wpipe->pipe_map.kva, wpipe->pipe_map.ms,
		wpipe->pipe_map.npages);

/*
 * and update the uio data
 */

	uio->uio_iov->iov_len -= size;
	uio->uio_iov->iov_base += size;
	if (uio->uio_iov->iov_len == 0)
		uio->uio_iov++;
	uio->uio_resid -= size;
	uio->uio_offset += size;
	return 0;
}

/*
 * unmap and unwire the process buffer
 */
static void
pipe_destroy_write_buffer(wpipe)
struct pipe *wpipe;
{
	int i;
	if (wpipe->pipe_map.kva) {
		pmap_qremove(wpipe->pipe_map.kva, wpipe->pipe_map.npages);

		if (amountpipekva > MAXPIPEKVA) {
			vm_offset_t kva = wpipe->pipe_map.kva;
			wpipe->pipe_map.kva = 0;
			kmem_free(kernel_map, kva,
				wpipe->pipe_buffer.size + PAGE_SIZE);
			amountpipekva -= wpipe->pipe_buffer.size + PAGE_SIZE;
		}
	}
	for (i=0;i<wpipe->pipe_map.npages;i++)
		vm_page_unwire(wpipe->pipe_map.ms[i]);
}

/*
 * In the case of a signal, the writing process might go away.  This
 * code copies the data into the circular buffer so that the source
 * pages can be freed without loss of data.
 */
static void
pipe_clone_write_buffer(wpipe)
struct pipe *wpipe;
{
	int size;
	int pos;

	size = wpipe->pipe_map.cnt;
	pos = wpipe->pipe_map.pos;
	bcopy((caddr_t) wpipe->pipe_map.kva+pos,
			(caddr_t) wpipe->pipe_buffer.buffer,
			size);

	wpipe->pipe_buffer.in = size;
	wpipe->pipe_buffer.out = 0;
	wpipe->pipe_buffer.cnt = size;
	wpipe->pipe_state &= ~PIPE_DIRECTW;

	pipe_destroy_write_buffer(wpipe);
}

/*
 * This implements the pipe buffer write mechanism.  Note that only
 * a direct write OR a normal pipe write can be pending at any given time.
 * If there are any characters in the pipe buffer, the direct write will
 * be deferred until the receiving process grabs all of the bytes from
 * the pipe buffer.  Then the direct mapping write is set-up.
 */
static int
pipe_direct_write(wpipe, uio)
	struct pipe *wpipe;
	struct uio *uio;
{
	int error;
retry:
	while (wpipe->pipe_state & PIPE_DIRECTW) {
		if ( wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
		wpipe->pipe_state |= PIPE_WANTW;
		error = tsleep(wpipe,
				PRIBIO|PCATCH, "pipdww", 0);
		if (error)
			goto error1;
		if (wpipe->pipe_state & PIPE_EOF) {
			error = EPIPE;
			goto error1;
		}
	}
	wpipe->pipe_map.cnt = 0;	/* transfer not ready yet */
	if (wpipe->pipe_buffer.cnt > 0) {
		if ( wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
			
		wpipe->pipe_state |= PIPE_WANTW;
		error = tsleep(wpipe,
				PRIBIO|PCATCH, "pipdwc", 0);
		if (error)
			goto error1;
		if (wpipe->pipe_state & PIPE_EOF) {
			error = EPIPE;
			goto error1;
		}
		goto retry;
	}

	wpipe->pipe_state |= PIPE_DIRECTW;

	error = pipe_build_write_buffer(wpipe, uio);
	if (error) {
		wpipe->pipe_state &= ~PIPE_DIRECTW;
		goto error1;
	}

	error = 0;
	while (!error && (wpipe->pipe_state & PIPE_DIRECTW)) {
		if (wpipe->pipe_state & PIPE_EOF) {
			pipelock(wpipe, 0);
			pipe_destroy_write_buffer(wpipe);
			pipeunlock(wpipe);
			pipeselwakeup(wpipe);
			error = EPIPE;
			goto error1;
		}
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
		pipeselwakeup(wpipe);
		error = tsleep(wpipe, PRIBIO|PCATCH, "pipdwt", 0);
	}

	pipelock(wpipe,0);
	if (wpipe->pipe_state & PIPE_DIRECTW) {
		/*
		 * this bit of trickery substitutes a kernel buffer for
		 * the process that might be going away.
		 */
		pipe_clone_write_buffer(wpipe);
	} else {
		pipe_destroy_write_buffer(wpipe);
	}
	pipeunlock(wpipe);
	return error;

error1:
	wakeup(wpipe);
	return error;
}
#endif
	
static int
pipe_write(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{
	int error = 0;
	int orig_resid;

	struct pipe *wpipe, *rpipe;

	rpipe = (struct pipe *) fp->f_data;
	wpipe = rpipe->pipe_peer;

	/*
	 * detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if ((wpipe == NULL) || (wpipe->pipe_state & PIPE_EOF)) {
		return EPIPE;
	}

	/*
	 * If it is advantageous to resize the pipe buffer, do
	 * so.
	 */
	if ((uio->uio_resid > PIPE_SIZE) &&
		(nbigpipe < LIMITBIGPIPES) &&
		(wpipe->pipe_state & PIPE_DIRECTW) == 0 &&
		(wpipe->pipe_buffer.size <= PIPE_SIZE) &&
		(wpipe->pipe_buffer.cnt == 0)) {

		if (wpipe->pipe_buffer.buffer) {
			amountpipekva -= wpipe->pipe_buffer.size;
			kmem_free(kernel_map,
				(vm_offset_t)wpipe->pipe_buffer.buffer,
				wpipe->pipe_buffer.size);
		}

#ifndef PIPE_NODIRECT
		if (wpipe->pipe_map.kva) {
			amountpipekva -= wpipe->pipe_buffer.size + PAGE_SIZE;
			kmem_free(kernel_map,
				wpipe->pipe_map.kva,
				wpipe->pipe_buffer.size + PAGE_SIZE);
		}
#endif

		wpipe->pipe_buffer.in = 0;
		wpipe->pipe_buffer.out = 0;
		wpipe->pipe_buffer.cnt = 0;
		wpipe->pipe_buffer.size = BIG_PIPE_SIZE;
		wpipe->pipe_buffer.buffer = NULL;
		++nbigpipe;

#ifndef PIPE_NODIRECT
		wpipe->pipe_map.cnt = 0;
		wpipe->pipe_map.kva = 0;
		wpipe->pipe_map.pos = 0;
		wpipe->pipe_map.npages = 0;
#endif

	}
		

	if( wpipe->pipe_buffer.buffer == NULL) {
		if ((error = pipelock(wpipe,1)) == 0) {
			pipespace(wpipe);
			pipeunlock(wpipe);
		} else {
			return error;
		}
	}

	++wpipe->pipe_busy;
	orig_resid = uio->uio_resid;
	while (uio->uio_resid) {
		int space;
#ifndef PIPE_NODIRECT
		/*
		 * If the transfer is large, we can gain performance if
		 * we do process-to-process copies directly.
		 * If the write is non-blocking, we don't use the
		 * direct write mechanism.
		 */
		if ((uio->uio_iov->iov_len >= PIPE_MINDIRECT) &&
		    (fp->f_flag & FNONBLOCK) == 0 &&
			(wpipe->pipe_map.kva || (amountpipekva < LIMITPIPEKVA)) &&
			(uio->uio_iov->iov_len >= PIPE_MINDIRECT)) {
			error = pipe_direct_write( wpipe, uio);
			if (error) {
				break;
			}
			continue;
		}
#endif

		/*
		 * Pipe buffered writes cannot be coincidental with
		 * direct writes.  We wait until the currently executing
		 * direct write is completed before we start filling the
		 * pipe buffer.
		 */
	retrywrite:
		while (wpipe->pipe_state & PIPE_DIRECTW) {
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}
			error = tsleep(wpipe,
					PRIBIO|PCATCH, "pipbww", 0);
			if (error)
				break;
		}

		space = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;

		/* Writes of size <= PIPE_BUF must be atomic. */
		/* XXX perhaps they need to be contiguous to be atomic? */
		if ((space < uio->uio_resid) && (orig_resid <= PIPE_BUF))
			space = 0;

		if (space > 0 && (wpipe->pipe_buffer.cnt < PIPE_SIZE)) {
			/*
			 * This set the maximum transfer as a segment of
			 * the buffer.
			 */
			int size = wpipe->pipe_buffer.size - wpipe->pipe_buffer.in;
			/*
			 * space is the size left in the buffer
			 */
			if (size > space)
				size = space;
			/*
			 * now limit it to the size of the uio transfer
			 */
			if (size > uio->uio_resid)
				size = uio->uio_resid;
			if ((error = pipelock(wpipe,1)) == 0) {
				/*
				 * It is possible for a direct write to
				 * slip in on us... handle it here...
				 */
				if (wpipe->pipe_state & PIPE_DIRECTW) {
					pipeunlock(wpipe);
					goto retrywrite;
				}
				error = uiomove( &wpipe->pipe_buffer.buffer[wpipe->pipe_buffer.in], 
					size, uio);
				pipeunlock(wpipe);
			}
			if (error)
				break;

			wpipe->pipe_buffer.in += size;
			if (wpipe->pipe_buffer.in >= wpipe->pipe_buffer.size)
				wpipe->pipe_buffer.in = 0;

			wpipe->pipe_buffer.cnt += size;
		} else {
			/*
			 * If the "read-side" has been blocked, wake it up now.
			 */
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}

			/*
			 * don't block on non-blocking I/O
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
				break;
			}

			/*
			 * We have no more space and have something to offer,
			 * wake up select/poll.
			 */
			pipeselwakeup(wpipe);

			wpipe->pipe_state |= PIPE_WANTW;
			if (error = tsleep(wpipe, (PRIBIO+1)|PCATCH, "pipewr", 0)) {
				break;
			}
			/*
			 * If read side wants to go away, we just issue a signal
			 * to ourselves.
			 */
			if (wpipe->pipe_state & PIPE_EOF) {
				error = EPIPE;
				break;
			}	
		}
	}

	--wpipe->pipe_busy;
	if ((wpipe->pipe_busy == 0) &&
		(wpipe->pipe_state & PIPE_WANT)) {
		wpipe->pipe_state &= ~(PIPE_WANT|PIPE_WANTR);
		wakeup(wpipe);
	} else if (wpipe->pipe_buffer.cnt > 0) {
		/*
		 * If we have put any characters in the buffer, we wake up
		 * the reader.
		 */
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
	}

	/*
	 * Don't return EPIPE if I/O was successful
	 */
	if ((wpipe->pipe_buffer.cnt == 0) &&
		(uio->uio_resid == 0) &&
		(error == EPIPE))
		error = 0;

	if (error == 0)
		getnanotime(&wpipe->pipe_mtime);

	/*
	 * We have something to offer,
	 * wake up select/poll.
	 */
	if (wpipe->pipe_buffer.cnt)
		pipeselwakeup(wpipe);

	return error;
}

/*
 * we implement a very minimal set of ioctls for compatibility with sockets.
 */
int
pipe_ioctl(fp, cmd, data, p)
	struct file *fp;
	u_long cmd;
	register caddr_t data;
	struct proc *p;
{
	register struct pipe *mpipe = (struct pipe *)fp->f_data;

	switch (cmd) {

	case FIONBIO:
		return (0);

	case FIOASYNC:
		if (*(int *)data) {
			mpipe->pipe_state |= PIPE_ASYNC;
		} else {
			mpipe->pipe_state &= ~PIPE_ASYNC;
		}
		return (0);

	case FIONREAD:
		if (mpipe->pipe_state & PIPE_DIRECTW)
			*(int *)data = mpipe->pipe_map.cnt;
		else
			*(int *)data = mpipe->pipe_buffer.cnt;
		return (0);

	case TIOCSPGRP:
		mpipe->pipe_pgid = *(int *)data;
		return (0);

	case TIOCGPGRP:
		*(int *)data = mpipe->pipe_pgid;
		return (0);

	}
	return (ENOTTY);
}

int
pipe_poll(fp, events, cred, p)
	struct file *fp;
	int events;
	struct ucred *cred;
	struct proc *p;
{
	register struct pipe *rpipe = (struct pipe *)fp->f_data;
	struct pipe *wpipe;
	int revents = 0;

	wpipe = rpipe->pipe_peer;
	if (events & (POLLIN | POLLRDNORM))
		if ((rpipe->pipe_state & PIPE_DIRECTW) ||
		    (rpipe->pipe_buffer.cnt > 0) ||
		    (rpipe->pipe_state & PIPE_EOF))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (wpipe == NULL || (wpipe->pipe_state & PIPE_EOF) ||
		    ((wpipe->pipe_state & PIPE_DIRECTW) == 0) &&
		     (wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt) >= PIPE_BUF)
			revents |= events & (POLLOUT | POLLWRNORM);

	if ((rpipe->pipe_state & PIPE_EOF) ||
	    (wpipe == NULL) ||
	    (wpipe->pipe_state & PIPE_EOF))
		revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM)) {
			selrecord(p, &rpipe->pipe_sel);
			rpipe->pipe_state |= PIPE_SEL;
		}

		if (events & (POLLOUT | POLLWRNORM)) {
			selrecord(p, &wpipe->pipe_sel);
			wpipe->pipe_state |= PIPE_SEL;
		}
	}

	return (revents);
}

int
pipe_stat(pipe, ub)
	register struct pipe *pipe;
	register struct stat *ub;
{
	bzero((caddr_t)ub, sizeof (*ub));
	ub->st_mode = S_IFIFO;
	ub->st_blksize = pipe->pipe_buffer.size;
	ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = (ub->st_size + ub->st_blksize - 1) / ub->st_blksize;
	ub->st_atimespec = pipe->pipe_atime;
	ub->st_mtimespec = pipe->pipe_mtime;
	ub->st_ctimespec = pipe->pipe_ctime;
	/*
	 * Left as 0: st_dev, st_ino, st_nlink, st_uid, st_gid, st_rdev,
	 * st_flags, st_gen.
	 * XXX (st_dev, st_ino) should be unique.
	 */
	return 0;
}

/* ARGSUSED */
static int
pipe_close(fp, p)
	struct file *fp;
	struct proc *p;
{
	struct pipe *cpipe = (struct pipe *)fp->f_data;

	pipeclose(cpipe);
	fp->f_data = NULL;
	return 0;
}

/*
 * shutdown the pipe
 */
static void
pipeclose(cpipe)
	struct pipe *cpipe;
{
	struct pipe *ppipe;
	if (cpipe) {
		
		pipeselwakeup(cpipe);

		/*
		 * If the other side is blocked, wake it up saying that
		 * we want to close it down.
		 */
		while (cpipe->pipe_busy) {
			wakeup(cpipe);
			cpipe->pipe_state |= PIPE_WANT|PIPE_EOF;
			tsleep(cpipe, PRIBIO, "pipecl", 0);
		}

		/*
		 * Disconnect from peer
		 */
		if (ppipe = cpipe->pipe_peer) {
			pipeselwakeup(ppipe);

			ppipe->pipe_state |= PIPE_EOF;
			wakeup(ppipe);
			ppipe->pipe_peer = NULL;
		}

		/*
		 * free resources
		 */
		if (cpipe->pipe_buffer.buffer) {
			if (cpipe->pipe_buffer.size > PIPE_SIZE)
				--nbigpipe;
			amountpipekva -= cpipe->pipe_buffer.size;
			kmem_free(kernel_map,
				(vm_offset_t)cpipe->pipe_buffer.buffer,
				cpipe->pipe_buffer.size);
		}
#ifndef PIPE_NODIRECT
		if (cpipe->pipe_map.kva) {
			amountpipekva -= cpipe->pipe_buffer.size + PAGE_SIZE;
			kmem_free(kernel_map,
				cpipe->pipe_map.kva,
				cpipe->pipe_buffer.size + PAGE_SIZE);
		}
#endif
		zfree(pipe_zone, cpipe);
	}
}
