/*-
 * Copyright (c) 2001 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $Id$
 * $FreeBSD$
 */

/*
 * This file contains part of LOMAC's interface to the kernel.  This
 * part allows LOMAC to monitor (unnamed) pipe read and write
 * operations by interposing control on the kernel's pipeops vector.
 *
 * The pipeops vector is defined in kern/sys_pipe.c.
 * 
 * USAGE:
 *
 * The LOMAC LKM should call lomac_initialize_pipes() at LKM load time.
 * This function turns unnamed pipe interposition on by modifying
 * the function addresses in pipeops.
 *
 * Once the LOMAC LKM turns interposition on, all reads and writes
 * will pass through this file's monitoring functions.
 *
 * This file provides a lomac_uninitialize_pipes() function which
 * turns unnamed pipe interposition off by restoring pipeops to
 * its original unmodified state.  Once the LOMAC LKM turns
 * interposition off, subsequent unnamed pipe reads and writes
 * will not pass through this file's monitoring functions.
 *
 * HOW LOMAC HANDLES PIPES: 
 *
 * (This text describes how LOMAC handles (unnamed) pipes in terms of
 * abstract architecture-independent concepts.)  LOMAC does not treat
 * pipes as objects, as it does files.  When the kernel creates a new
 * pipe, LOMAC assigns it the highest level.  Whenever a process
 * writes to the pipe, LOMAC reduces the pipe's level to match the
 * level of the writing process.  Whenever a process reads from a
 * pipe, LOMAC reduces the level of the reading process to match the
 * pipe's level.  As a result, if a high-level process reads the
 * output of a low-level process through a pipe, the reading process
 * will wind up at the low level.
 * 
 * It takes two `struct pipe's to make a pipe.  We set the level
 * information in both `struct pipes', and keep them synchronized.
 *                                                                        
 * This code presently relies on the one-big-kernel-lock to
 * synchronize its access to the `pipe_state' field of each `struct
 * pipe'.
 *                                                                        
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/pipe.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include "lomac.h"
#include "kernel_interface.h"
#include "kernel_monitor.h"
#include "kernel_pipe.h"


/* `pipeops' is the kernel's pipe operations vector for the file       *
 * structure.  All reads and writes to pipes call through this vector. */
extern struct fileops pipeops;           /* defined in kern/sys_pipe.c */


/* These vars store the original addresses of the pipeops read and *
 * write operations, so we can call them, and even restore them    *
 * later if we want to.                                            */
static int (*pipe_read_orig)(struct file *, struct uio *, struct ucred *,
    int, struct thread *);
static int (*pipe_write_orig)(struct file *, struct uio *, struct ucred *,
    int, struct thread *);


/* declarations of functions private to this module: */
static int lomac_pipe_read(struct file *, struct uio *, struct ucred *, 
    int, struct thread *);
static int lomac_pipe_write(struct file *, struct uio *, struct ucred *, 
    int, struct thread *);

/* -------------------- public functions ---------------------------- */

/* lomac_initialize_pipes()
 *
 * in:     nothing
 * out:    nothing
 * return: 0
 *
 * Turns pipe interposition on by replacing the pipe_read() and pipe_write()
 * operations in the kernel's pipeops vector with lomac_pipe_read() and
 * lomac_pipe_write().  Saves the addresses of the original operations
 * so other functions can call them, and so pipe_interposition_off()
 * can restore the pipeops vector to its original unmodified state.
 *
 */

int
lomac_initialize_pipes(void) {

	pipe_read_orig  = pipeops.fo_read;
	pipeops.fo_read = lomac_pipe_read;
	pipe_write_orig  = pipeops.fo_write;
	pipeops.fo_write = lomac_pipe_write;
	return (0);
} /* lomac_initialize_pipes() */


/* lomac_uninitialize_pipes()
 *
 * in:     nothing
 * out:    nothing
 * return: 0
 *
 * Turns pipe interposition off by restoring the pipeops vector to its
 * original unmodified state.
 *
 * See note at top of file regarding this function and unloading the
 * LOMAC LKM.
 *
 */

int
lomac_uninitialize_pipes(void) {

	KASSERT(pipe_read_orig,  ("LOMAC:pipe interpositon off before on"));
	KASSERT(pipe_write_orig, ("LOMAC:pipe interpositon off before on"));
	pipeops.fo_read  = pipe_read_orig;
	pipeops.fo_write = pipe_write_orig;
	return (0);
} /* lomac_uninitialize_pipes() */



/* ------------------- private functions --------------------------- */

#ifndef MIN
#define	MIN(lo, mac) ((lo) < (mac) ? (lo) : (mac))
#endif

/* lomac_pipe_read()
 *
 * Passes the read operation down to pipe_read_orig().  If
 * pipe_read_orig() returns success, examines the level of the pipe
 * and the reading process.  If the reading process has a higher
 * level, reduces the level of the process to equal the pipe's level.
 *
 */

static int
lomac_pipe_read(struct file *fp, struct uio *uio, struct ucred *cred, 
    int flags, struct thread *td) {
	lomac_object_t read_pipe;     /* attrs are in read end of pipe */
	struct uio kuio;
	struct iovec kiov;
	void *buf;
	int len;
	int ret_val;                            /* holds return values */

	len = MIN(uio->uio_resid, BIG_PIPE_SIZE);
	kiov.iov_base = buf = malloc(len, M_TEMP, M_WAITOK);
	kiov.iov_len = len;
	kuio.uio_iov = &kiov;
	kuio.uio_iovcnt = 1;
	kuio.uio_offset = 0;
	kuio.uio_resid = len;
	kuio.uio_segflg = UIO_SYSSPACE;
	kuio.uio_rw = UIO_READ;
	kuio.uio_td = td;
	ret_val = pipe_read_orig(fp, &kuio, cred, flags, td);
	if (ret_val == 0) {
		read_pipe.lo_type = LO_TYPE_PIPE;
		read_pipe.lo_object.pipe = (struct pipe *)fp->f_data;
		(void)monitor_read_object(td->td_proc, &read_pipe);
		ret_val = uiomove(buf, len - kuio.uio_resid, uio);
	}
	free(buf, M_TEMP);
	return (ret_val);
} /* lomac_pipe_read() */ 


/* lomac_pipe_write()
 *
 * Passes the write operation down to pipe_write_orig().  If
 * pipe_write_orig() returns success, examines the level of the pipe
 * and the writing process.  If the pipe has a higher level than the
 * writing process, this function reduces the pipe's level to equal
 * the level of the writing process.
 *
 */

static int
lomac_pipe_write(struct file *fp, struct uio *uio, struct ucred *cred, 
    int flags, struct thread *td) {
	lomac_object_t pipe;   
	int ret_val;                            /* holds return values */

	pipe.lo_type = LO_TYPE_PIPE;
	pipe.lo_object.pipe = (struct pipe *)fp->f_data;
	ret_val = monitor_pipe_write(td->td_proc, &pipe);
	if (ret_val == 0)
		ret_val = pipe_write_orig(fp, uio, cred, flags, td);

	return (ret_val);
} /* lomac_pipe_write() */ 
