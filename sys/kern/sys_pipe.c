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
 * 4. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is allowed if this notation is included.
 * 5. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $Id: sys_pipe.c,v 1.1 1996/01/28 23:38:26 dyson Exp $
 */

#ifndef OLD_PIPE

/*
 * This file contains a high-performance replacement for the socket-based
 * pipes scheme originally used in FreeBSD/4.4Lite.  It does not support
 * all features of sockets, but does do everything that pipes normally
 * do.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/pipe.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>
#include <vm/vm_param.h>
#include <vm/lock.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

static int pipe_read __P((struct file *fp, struct uio *uio, 
		struct ucred *cred));
static int pipe_write __P((struct file *fp, struct uio *uio, 
		struct ucred *cred));
static int pipe_close __P((struct file *fp, struct proc *p));
static int pipe_select __P((struct file *fp, int which, struct proc *p));
static int pipe_ioctl __P((struct file *fp, int cmd, caddr_t data, struct proc *p));

static struct fileops pipeops =
    { pipe_read, pipe_write, pipe_ioctl, pipe_select, pipe_close };

/*
 * Default pipe buffer size(s), this can be kind-of large now because pipe
 * space is pageable.  The pipe code will try to maintain locality of
 * reference for performance reasons, so small amounts of outstanding I/O
 * will not wipe the cache.
 */
#define PIPESIZE (16384)
#define MINPIPESIZE (PIPESIZE/3)
#define MAXPIPESIZE (2*PIPESIZE/3)

static void pipeclose __P((struct pipe *cpipe));
static void pipebufferinit __P((struct pipe *cpipe));
static void pipeinit __P((struct pipe *cpipe));
static __inline int pipelock __P((struct pipe *cpipe));
static __inline void pipeunlock __P((struct pipe *cpipe));

/*
 * The pipe system call for the DTYPE_PIPE type of pipes
 */

/* ARGSUSED */
int
pipe(p, uap, retval)
	struct proc *p;
	struct pipe_args /* {
		int	dummy;
	} */ *uap;
	int retval[];
{
	register struct filedesc *fdp = p->p_fd;
	struct file *rf, *wf;
	struct pipe *rpipe, *wpipe;
	int fd, error;

	rpipe = malloc( sizeof (*rpipe), M_TEMP, M_WAITOK);
	pipeinit(rpipe);
	wpipe = malloc( sizeof (*wpipe), M_TEMP, M_WAITOK);
	pipeinit(wpipe);

	error = falloc(p, &rf, &fd);
	if (error)
		goto free2;
	retval[0] = fd;
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
	retval[1] = fd;

	rpipe->pipe_peer = wpipe;
	wpipe->pipe_peer = rpipe;

	return (0);
free3:
	ffree(rf);
	fdp->fd_ofiles[retval[0]] = 0;
free2:
	(void)pipeclose(wpipe);
free1:
	(void)pipeclose(rpipe);
	return (error);
}

/*
 * initialize and allocate VM and memory for pipe
 */
static void
pipeinit(cpipe)
	struct pipe *cpipe;
{
	int npages, error;

	npages = round_page(PIPESIZE)/PAGE_SIZE;

	/*
	 * Create an object, I don't like the idea of paging to/from
	 * kernel_object.
	 */
	cpipe->pipe_buffer.object = vm_object_allocate(OBJT_DEFAULT, npages);
	cpipe->pipe_buffer.buffer = (caddr_t) vm_map_min(kernel_map);

	/*
	 * Insert the object into the kernel map, and allocate kva for it.
	 * The map entry is, by default, pageable.
	 */
	error = vm_map_find(kernel_map, cpipe->pipe_buffer.object, 0,
		(vm_offset_t *) &cpipe->pipe_buffer.buffer, PIPESIZE, 1,
		VM_PROT_ALL, VM_PROT_ALL, 0);

	if (error != KERN_SUCCESS)
		panic("pipeinit: cannot allocate pipe -- out of kvm -- code = %d", error);

	cpipe->pipe_buffer.in = 0;
	cpipe->pipe_buffer.out = 0;
	cpipe->pipe_buffer.cnt = 0;
	cpipe->pipe_buffer.size = PIPESIZE;

	cpipe->pipe_state = 0;
	cpipe->pipe_peer = NULL;
	cpipe->pipe_busy = 0;
	cpipe->pipe_ctime = time;
	cpipe->pipe_atime = time;
	cpipe->pipe_mtime = time;
	bzero(&cpipe->pipe_sel, sizeof cpipe->pipe_sel);
}


/*
 * lock a pipe for I/O, blocking other access
 */
static __inline int
pipelock(cpipe)
	struct pipe *cpipe;
{
	while (cpipe->pipe_state & PIPE_LOCK) {
		cpipe->pipe_state |= PIPE_LWANT;
		if (tsleep( &cpipe->pipe_state, PRIBIO|PCATCH, "pipelk", 0)) {
			return ERESTART;
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
		wakeup(&cpipe->pipe_state);
	}
	return;
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

	++rpipe->pipe_busy;
	while (uio->uio_resid) {
		if (rpipe->pipe_buffer.cnt > 0) {
			int size = rpipe->pipe_buffer.size - rpipe->pipe_buffer.out;
			if (size > rpipe->pipe_buffer.cnt)
				size = rpipe->pipe_buffer.cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;
			if ((error = pipelock(rpipe)) == 0) {
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
			rpipe->pipe_atime = time;
		} else {
			/*
			 * detect EOF condition
			 */
			if (rpipe->pipe_state & PIPE_EOF) {
				break;
			}
			/*
			 * If the "write-side" has been blocked, wake it up now.
			 */
			if (rpipe->pipe_state & PIPE_WANTW) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}
			if ((nread > 0) || (rpipe->pipe_state & PIPE_NBIO))
				break;
			if (rpipe->pipe_peer == NULL)
				break;

			/*
			 * If there is no more to read in the pipe, reset
			 * it's pointers to the beginning.  This improves
			 * cache hit stats.
			 */
		
			if ((error = pipelock(rpipe)) == 0) {
				if (rpipe->pipe_buffer.cnt == 0) {
					rpipe->pipe_buffer.in = 0;
					rpipe->pipe_buffer.out = 0;
				}
				pipeunlock(rpipe);
			} else {
				break;
			}
			rpipe->pipe_state |= PIPE_WANTR;
			if (tsleep(rpipe, PRIBIO|PCATCH, "piperd", 0)) {
				error = ERESTART;
				break;
			}
		}
	}

	--rpipe->pipe_busy;
	if ((rpipe->pipe_busy == 0) && (rpipe->pipe_state & PIPE_WANT)) {
		rpipe->pipe_state &= ~(PIPE_WANT|PIPE_WANTW);
		wakeup(rpipe);
	} else if (rpipe->pipe_buffer.cnt < MINPIPESIZE) {
		/*
		 * If there is no more to read in the pipe, reset
		 * it's pointers to the beginning.  This improves
		 * cache hit stats.
		 */
		if ((error == 0) && (error = pipelock(rpipe)) == 0) {
			if (rpipe->pipe_buffer.cnt == 0) {
				rpipe->pipe_buffer.in = 0;
				rpipe->pipe_buffer.out = 0;
			}
			pipeunlock(rpipe);
		}

		/*
		 * If the "write-side" has been blocked, wake it up now.
		 */
		if (rpipe->pipe_state & PIPE_WANTW) {
			rpipe->pipe_state &= ~PIPE_WANTW;
			wakeup(rpipe);
		}
	}
	if (rpipe->pipe_state & PIPE_SEL) {
		rpipe->pipe_state &= ~PIPE_SEL;
		selwakeup(&rpipe->pipe_sel);
	}
	return error;
}

/* ARGSUSED */
static int
pipe_write(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{
	struct pipe *rpipe = (struct pipe *) fp->f_data;
	struct pipe *wpipe = rpipe->pipe_peer;
	int error = 0;

	/*
	 * detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if (wpipe == NULL || (wpipe->pipe_state & PIPE_EOF)) {
		psignal(curproc, SIGPIPE);
		return 0;
	}

	++wpipe->pipe_busy;
	while (uio->uio_resid) {
		int space = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;
		if (space > 0) {
			int size = wpipe->pipe_buffer.size - wpipe->pipe_buffer.in;
			if (size > space)
				size = space;
			if (size > uio->uio_resid)
				size = uio->uio_resid;
			if ((error = pipelock(wpipe)) == 0) {
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
			wpipe->pipe_mtime = time;
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
			if (wpipe->pipe_state & PIPE_NBIO) {
				break;
			}
			wpipe->pipe_state |= PIPE_WANTW;
			if (tsleep(wpipe, (PRIBIO+1)|PCATCH, "pipewr", 0)) {
				error = ERESTART;
				break;
			}
			/*
			 * If read side wants to go away, we just issue a signal
			 * to ourselves.
			 */
			if (wpipe->pipe_state & PIPE_EOF) {
				psignal(curproc, SIGPIPE);
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
	if (wpipe->pipe_state & PIPE_SEL) {
		wpipe->pipe_state &= ~PIPE_SEL;
		selwakeup(&wpipe->pipe_sel);
	}
	return error;
}

/*
 * we implement a very minimal set of ioctls for compatibility with sockets.
 */
int
pipe_ioctl(fp, cmd, data, p)
	struct file *fp;
	int cmd;
	register caddr_t data;
	struct proc *p;
{
	register struct pipe *mpipe = (struct pipe *)fp->f_data;

	switch (cmd) {

	case FIONBIO:
		if (*(int *)data)
			mpipe->pipe_state |= PIPE_NBIO;
		else
			mpipe->pipe_state &= ~PIPE_NBIO;
		return (0);

	case FIOASYNC:
		if (*(int *)data) {
			mpipe->pipe_state |= PIPE_ASYNC;
		} else {
			mpipe->pipe_state &= ~PIPE_ASYNC;
		}
		return (0);

	case FIONREAD:
		*(int *)data = mpipe->pipe_buffer.cnt;
		return (0);

	case SIOCSPGRP:
		mpipe->pipe_pgid = *(int *)data;
		return (0);

	case SIOCGPGRP:
		*(int *)data = mpipe->pipe_pgid;
		return (0);

	}
	return ENOSYS;
}

int
pipe_select(fp, which, p)
	struct file *fp;
	int which;
	struct proc *p;
{
	register struct pipe *rpipe = (struct pipe *)fp->f_data;
	struct pipe *wpipe;
	register int s = splnet();

	wpipe = rpipe->pipe_peer;
	switch (which) {

	case FREAD:
		if (rpipe->pipe_buffer.cnt > 0) {
			splx(s);
			return (1);
		}
		selrecord(p, &rpipe->pipe_sel);
		rpipe->pipe_state |= PIPE_SEL;
		break;

	case FWRITE:
		if (wpipe == 0) {
			splx(s);
			return (1);
		}
		if (wpipe->pipe_buffer.cnt < wpipe->pipe_buffer.size) {
			splx(s);
			return (1);
		}
		selrecord(p, &wpipe->pipe_sel);
		wpipe->pipe_state |= PIPE_SEL;
		break;

	case 0:
		selrecord(p, &rpipe->pipe_sel);
		rpipe->pipe_state |= PIPE_SEL;
		break;
	}
	splx(s);
	return (0);
}

int
pipe_stat(pipe, ub)
	register struct pipe *pipe;
	register struct stat *ub;
{
	bzero((caddr_t)ub, sizeof (*ub));
	ub->st_mode = S_IFSOCK;
	ub->st_blksize = pipe->pipe_buffer.size / 2;
	ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = (ub->st_size + ub->st_blksize - 1) / ub->st_blksize;
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_atime, &ub->st_atimespec);
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_mtime, &ub->st_mtimespec);
	TIMEVAL_TO_TIMESPEC(&pipe->pipe_ctime, &ub->st_ctimespec);
	return 0;
}

/* ARGSUSED */
static int
pipe_close(fp, p)
	struct file *fp;
	struct proc *p;
{
	int error = 0;
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
	if (cpipe) {
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
		if (cpipe->pipe_peer) {
			cpipe->pipe_peer->pipe_state |= PIPE_EOF;
			wakeup(cpipe->pipe_peer);
			cpipe->pipe_peer->pipe_peer = NULL;
		}

		/*
		 * free resources
		 */
		kmem_free(kernel_map, (vm_offset_t)cpipe->pipe_buffer.buffer,
			cpipe->pipe_buffer.size);
		free(cpipe, M_TEMP);
	}
}
#endif
