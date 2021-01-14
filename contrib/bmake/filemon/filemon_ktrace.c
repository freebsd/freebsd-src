/*	$NetBSD: filemon_ktrace.c,v 1.12 2021/01/10 23:59:53 rillig Exp $	*/

/*-
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _KERNTYPES		/* register_t */

#include "filemon.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/rbtree.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <sys/ktrace.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AT_CWD
#define AT_CWD -1
#endif

struct filemon;
struct filemon_key;
struct filemon_state;

typedef struct filemon_state *filemon_syscall_t(struct filemon *,
    const struct filemon_key *, const struct ktr_syscall *);

static filemon_syscall_t filemon_sys_chdir;
static filemon_syscall_t filemon_sys_execve;
static filemon_syscall_t filemon_sys_exit;
static filemon_syscall_t filemon_sys_fork;
static filemon_syscall_t filemon_sys_link;
static filemon_syscall_t filemon_sys_open;
static filemon_syscall_t filemon_sys_openat;
static filemon_syscall_t filemon_sys_symlink;
static filemon_syscall_t filemon_sys_unlink;
static filemon_syscall_t filemon_sys_rename;

static filemon_syscall_t *const filemon_syscalls[] = {
	[SYS_chdir] = &filemon_sys_chdir,
	[SYS_execve] = &filemon_sys_execve,
	[SYS_exit] = &filemon_sys_exit,
	[SYS_fork] = &filemon_sys_fork,
	[SYS_link] = &filemon_sys_link,
	[SYS_open] = &filemon_sys_open,
	[SYS_openat] = &filemon_sys_openat,
	[SYS_symlink] = &filemon_sys_symlink,
	[SYS_unlink] = &filemon_sys_unlink,
	[SYS_rename] = &filemon_sys_rename,
};

struct filemon {
	int			ktrfd;	/* kernel writes ktrace events here */
	FILE			*in;	/* we read ktrace events from here */
	FILE			*out;	/* we write filemon events to here */
	rb_tree_t		active;
	pid_t			child;

	/* I/O state machine.  */
	enum {
		FILEMON_START = 0,
		FILEMON_HEADER,
		FILEMON_PAYLOAD,
		FILEMON_ERROR,
	}			state;
	unsigned char		*p;
	size_t			resid;

	/* I/O buffer.  */
	struct ktr_header	hdr;
	union {
		struct ktr_syscall	syscall;
		struct ktr_sysret	sysret;
		char			namei[PATH_MAX];
		unsigned char		buf[4096];
	}			payload;
};

struct filemon_state {
	struct filemon_key {
		pid_t		pid;
		lwpid_t		lid;
	}		key;
	struct rb_node	node;
	int		syscode;
	void		(*show)(struct filemon *, const struct filemon_state *,
			    const struct ktr_sysret *);
	unsigned	i;
	unsigned	npath;
	char		*path[/*npath*/];
};

/*ARGSUSED*/
static int
compare_filemon_states(void *cookie, const void *na, const void *nb)
{
	const struct filemon_state *Sa = na;
	const struct filemon_state *Sb = nb;

	if (Sa->key.pid < Sb->key.pid)
		return -1;
	if (Sa->key.pid > Sb->key.pid)
		return +1;
	if (Sa->key.lid < Sb->key.lid)
		return -1;
	if (Sa->key.lid > Sb->key.lid)
		return +1;
	return 0;
}

/*ARGSUSED*/
static int
compare_filemon_key(void *cookie, const void *n, const void *k)
{
	const struct filemon_state *S = n;
	const struct filemon_key *key = k;

	if (S->key.pid < key->pid)
		return -1;
	if (S->key.pid > key->pid)
		return +1;
	if (S->key.lid < key->lid)
		return -1;
	if (S->key.lid > key->lid)
		return +1;
	return 0;
}

static const rb_tree_ops_t filemon_rb_ops = {
	.rbto_compare_nodes = &compare_filemon_states,
	.rbto_compare_key = &compare_filemon_key,
	.rbto_node_offset = offsetof(struct filemon_state, node),
	.rbto_context = NULL,
};

/*
 * filemon_path()
 *
 *	Return a pointer to a constant string denoting the `path' of
 *	the filemon.
 */
const char *
filemon_path(void)
{

	return "ktrace";
}

/*
 * filemon_open()
 *
 *	Allocate a filemon descriptor.  Returns NULL and sets errno on
 *	failure.
 */
struct filemon *
filemon_open(void)
{
	struct filemon *F;
	int ktrpipe[2];
	int error;

	/* Allocate and zero a struct filemon object.  */
	F = calloc(1, sizeof *F);
	if (F == NULL)
		return NULL;

	/* Create a pipe for ktrace events.  */
	if (pipe2(ktrpipe, O_CLOEXEC|O_NONBLOCK) == -1) {
		error = errno;
		goto fail0;
	}

	/* Create a file stream for reading the ktrace events.  */
	if ((F->in = fdopen(ktrpipe[0], "r")) == NULL) {
		error = errno;
		goto fail1;
	}
	ktrpipe[0] = -1;	/* claimed by fdopen */

	/*
	 * Set the fd for writing ktrace events and initialize the
	 * rbtree.  The rest can be safely initialized to zero.
	 */
	F->ktrfd = ktrpipe[1];
	rb_tree_init(&F->active, &filemon_rb_ops);

	/* Success!  */
	return F;

	(void)fclose(F->in);
fail1:	(void)close(ktrpipe[0]);
	(void)close(ktrpipe[1]);
fail0:	free(F);
	errno = error;
	return NULL;
}

/*
 * filemon_closefd(F)
 *
 *	Internal subroutine to try to flush and close the output file.
 *	If F is not open for output, do nothing.  Never leaves F open
 *	for output even on failure.  Returns 0 on success; sets errno
 *	and return -1 on failure.
 */
static int
filemon_closefd(struct filemon *F)
{
	int error = 0;

	/* If we're not open, nothing to do.  */
	if (F->out == NULL)
		return 0;

	/*
	 * Flush it, close it, and null it unconditionally, but be
	 * careful to return the earliest error in errno.
	 */
	if (fflush(F->out) == EOF && error == 0)
		error = errno;
	if (fclose(F->out) == EOF && error == 0)
		error = errno;
	F->out = NULL;

	/* Set errno and return -1 if anything went wrong.  */
	if (error != 0) {
		errno = error;
		return -1;
	}

	/* Success!  */
	return 0;
}

/*
 * filemon_setfd(F, fd)
 *
 *	Cause filemon activity on F to be sent to fd.  Claims ownership
 *	of fd; caller should not use fd afterward, and any duplicates
 *	of fd may see their file positions changed.
 */
int
filemon_setfd(struct filemon *F, int fd)
{

	/*
	 * Close an existing output file if done.  Fail now if there's
	 * an error closing.
	 */
	if ((filemon_closefd(F)) == -1)
		return -1;
	assert(F->out == NULL);

	/* Open a file stream and claim ownership of the fd.  */
	if ((F->out = fdopen(fd, "a")) == NULL)
		return -1;

	/*
	 * Print the opening output.  Any failure will be deferred
	 * until closing.  For hysterical raisins, we show the parent
	 * pid, not the child pid.
	 */
	fprintf(F->out, "# filemon version 4\n");
	fprintf(F->out, "# Target pid %jd\n", (intmax_t)getpid());
	fprintf(F->out, "V 4\n");

	/* Success!  */
	return 0;
}

/*
 * filemon_setpid_parent(F, pid)
 *
 *	Set the traced pid, from the parent.  Never fails.
 */
void
filemon_setpid_parent(struct filemon *F, pid_t pid)
{

	F->child = pid;
}

/*
 * filemon_setpid_child(F, pid)
 *
 *	Set the traced pid, from the child.  Returns 0 on success; sets
 *	errno and returns -1 on failure.
 */
int
filemon_setpid_child(const struct filemon *F, pid_t pid)
{
	int ops, trpoints;

	ops = KTROP_SET|KTRFLAG_DESCEND;
	trpoints = KTRFACv2;
	trpoints |= KTRFAC_SYSCALL|KTRFAC_NAMEI|KTRFAC_SYSRET;
	trpoints |= KTRFAC_INHERIT;
	if (fktrace(F->ktrfd, ops, trpoints, pid) == -1)
		return -1;

	return 0;
}

/*
 * filemon_close(F)
 *
 *	Close F for output if necessary, and free a filemon descriptor.
 *	Returns 0 on success; sets errno and returns -1 on failure, but
 *	frees the filemon descriptor either way;
 */
int
filemon_close(struct filemon *F)
{
	struct filemon_state *S;
	int error = 0;

	/* Close for output.  */
	if (filemon_closefd(F) == -1 && error == 0)
		error = errno;

	/* Close the ktrace pipe.  */
	if (fclose(F->in) == EOF && error == 0)
		error = errno;
	if (close(F->ktrfd) == -1 && error == 0)
		error = errno;

	/* Free any active records.  */
	while ((S = RB_TREE_MIN(&F->active)) != NULL) {
		rb_tree_remove_node(&F->active, S);
		free(S);
	}

	/* Free the filemon descriptor.  */
	free(F);

	/* Set errno and return -1 if anything went wrong.  */
	if (error != 0) {
		errno = error;
		return -1;
	}

	/* Success!  */
	return 0;
}

/*
 * filemon_readfd(F)
 *
 *	Returns a file descriptor which will select/poll ready for read
 *	when there are filemon events to be processed by
 *	filemon_process, or -1 if anything has gone wrong.
 */
int
filemon_readfd(const struct filemon *F)
{

	if (F->state == FILEMON_ERROR)
		return -1;
	return fileno(F->in);
}

/*
 * filemon_dispatch(F)
 *
 *	Internal subroutine to dispatch a filemon ktrace event.
 *	Silently ignore events that we don't recognize.
 */
static void
filemon_dispatch(struct filemon *F)
{
	const struct filemon_key key = {
		.pid = F->hdr.ktr_pid,
		.lid = F->hdr.ktr_lid,
	};
	struct filemon_state *S;

	switch (F->hdr.ktr_type) {
	case KTR_SYSCALL: {
		struct ktr_syscall *call = &F->payload.syscall;
		struct filemon_state *S1;

		/* Validate the syscall code.  */
		if (call->ktr_code < 0 ||
		    (size_t)call->ktr_code >= __arraycount(filemon_syscalls) ||
		    filemon_syscalls[call->ktr_code] == NULL)
			break;

		/*
		 * Invoke the syscall-specific logic to create a new
		 * active state.
		 */
		S = (*filemon_syscalls[call->ktr_code])(F, &key, call);
		if (S == NULL)
			break;

		/*
		 * Insert the active state, or ignore it if there
		 * already is one.
		 *
		 * Collisions shouldn't happen because the states are
		 * keyed by <pid,lid>, in which syscalls should happen
		 * sequentially in CALL/RET pairs, but let's be
		 * defensive.
		 */
		S1 = rb_tree_insert_node(&F->active, S);
		if (S1 != S) {
			/* XXX Which one to drop?  */
			free(S);
			break;
		}
		break;
	}
	case KTR_NAMEI:
		/* Find an active syscall state, or drop it.  */
		S = rb_tree_find_node(&F->active, &key);
		if (S == NULL)
			break;
		/* Find the position of the next path, or drop it.  */
		if (S->i >= S->npath)
			break;
		/* Record the path.  */
		S->path[S->i++] = strndup(F->payload.namei,
		    sizeof F->payload.namei);
		break;
	case KTR_SYSRET: {
		struct ktr_sysret *ret = &F->payload.sysret;
		unsigned i;

		/* Find and remove an active syscall state, or drop it.  */
		S = rb_tree_find_node(&F->active, &key);
		if (S == NULL)
			break;
		rb_tree_remove_node(&F->active, S);

		/*
		 * If the active syscall state matches this return,
		 * invoke the syscall-specific logic to show a filemon
		 * event.
		 */
		/* XXX What to do if syscall code doesn't match?  */
		if (S->i == S->npath && S->syscode == ret->ktr_code)
			S->show(F, S, ret);

		/* Free the state now that it is no longer active.  */
		for (i = 0; i < S->i; i++)
			free(S->path[i]);
		free(S);
		break;
	}
	default:
		/* Ignore all other ktrace events.  */
		break;
	}
}

/*
 * filemon_process(F)
 *
 *	Process all pending events after filemon_readfd(F) has
 *	selected/polled ready for read.
 *
 *	Returns -1 on failure, 0 on end of events, and anything else if
 *	there may be more events.
 *
 *	XXX What about fairness to other activities in the event loop?
 *	If we stop while there's events buffered in F->in, then select
 *	or poll may not return ready even though there's work queued up
 *	in the buffer of F->in, but if we don't stop then ktrace events
 *	may overwhelm all other activity in the event loop.
 */
int
filemon_process(struct filemon *F)
{
	size_t nread;

top:	/* If the child has exited, nothing to do.  */
	/* XXX What if one thread calls exit while another is running?  */
	if (F->child == 0)
		return 0;

	/* If we're waiting for input, read some.  */
	if (F->resid > 0) {
		nread = fread(F->p, 1, F->resid, F->in);
		if (nread == 0) {
			if (feof(F->in) != 0)
				return 0;
			assert(ferror(F->in) != 0);
			/*
			 * If interrupted or would block, there may be
			 * more events.  Otherwise fail.
			 */
			if (errno == EAGAIN || errno == EINTR)
				return 1;
			F->state = FILEMON_ERROR;
			F->p = NULL;
			F->resid = 0;
			return -1;
		}
		assert(nread <= F->resid);
		F->p += nread;
		F->resid -= nread;
		if (F->resid > 0)	/* may be more events */
			return 1;
	}

	/* Process a state transition now that we've read a buffer.  */
	switch (F->state) {
	case FILEMON_START:	/* just started filemon; read header next */
		F->state = FILEMON_HEADER;
		F->p = (void *)&F->hdr;
		F->resid = sizeof F->hdr;
		goto top;
	case FILEMON_HEADER:	/* read header */
		/* Sanity-check ktrace header; then read payload.  */
		if (F->hdr.ktr_len < 0 ||
		    (size_t)F->hdr.ktr_len > sizeof F->payload) {
			F->state = FILEMON_ERROR;
			F->p = NULL;
			F->resid = 0;
			errno = EIO;
			return -1;
		}
		F->state = FILEMON_PAYLOAD;
		F->p = (void *)&F->payload;
		F->resid = (size_t)F->hdr.ktr_len;
		goto top;
	case FILEMON_PAYLOAD:	/* read header and payload */
		/* Dispatch ktrace event; then read next header.  */
		filemon_dispatch(F);
		F->state = FILEMON_HEADER;
		F->p = (void *)&F->hdr;
		F->resid = sizeof F->hdr;
		goto top;
	default:		/* paranoia */
		F->state = FILEMON_ERROR;
		/*FALLTHROUGH*/
	case FILEMON_ERROR:	/* persistent error indicator */
		F->p = NULL;
		F->resid = 0;
		errno = EIO;
		return -1;
	}
}

static struct filemon_state *
syscall_enter(
    const struct filemon_key *key, const struct ktr_syscall *call,
    unsigned npath,
    void (*show)(struct filemon *, const struct filemon_state *,
	const struct ktr_sysret *))
{
	struct filemon_state *S;
	unsigned i;

	S = calloc(1, offsetof(struct filemon_state, path[npath]));
	if (S == NULL)
		return NULL;
	S->key = *key;
	S->show = show;
	S->syscode = call->ktr_code;
	S->i = 0;
	S->npath = npath;
	for (i = 0; i < npath; i++)
		 S->path[i] = NULL; /* paranoia */

	return S;
}

static void
show_paths(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret, const char *prefix)
{
	unsigned i;

	/* Caller must ensure all paths have been specified.  */
	assert(S->i == S->npath);

	/*
	 * Ignore it if it failed or yielded EJUSTRETURN (-2), or if
	 * we're not producing output.
	 */
	if (ret->ktr_error != 0 && ret->ktr_error != -2)
		return;
	if (F->out == NULL)
		return;

	/*
	 * Print the prefix, pid, and paths -- with the paths quoted if
	 * there's more than one.
	 */
	fprintf(F->out, "%s %jd", prefix, (intmax_t)S->key.pid);
	for (i = 0; i < S->npath; i++) {
		const char *q = S->npath > 1 ? "'" : "";
		fprintf(F->out, " %s%s%s", q, S->path[i], q);
	}
	fprintf(F->out, "\n");
}

static void
show_retval(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret, const char *prefix)
{

	/*
	 * Ignore it if it failed or yielded EJUSTRETURN (-2), or if
	 * we're not producing output.
	 */
	if (ret->ktr_error != 0 && ret->ktr_error != -2)
		return;
	if (F->out == NULL)
		return;

	fprintf(F->out, "%s %jd %jd\n", prefix, (intmax_t)S->key.pid,
	    (intmax_t)ret->ktr_retval);
}

static void
show_chdir(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "C");
}

static void
show_execve(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "E");
}

static void
show_fork(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_retval(F, S, ret, "F");
}

static void
show_link(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "L"); /* XXX same as symlink */
}

static void
show_open_read(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "R");
}

static void
show_open_write(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "W");
}

static void
show_open_readwrite(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "R");
	show_paths(F, S, ret, "W");
}

static void
show_openat_read(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	if (S->path[0][0] != '/')
		show_paths(F, S, ret, "A");
	show_paths(F, S, ret, "R");
}

static void
show_openat_write(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	if (S->path[0][0] != '/')
		show_paths(F, S, ret, "A");
	show_paths(F, S, ret, "W");
}

static void
show_openat_readwrite(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	if (S->path[0][0] != '/')
		show_paths(F, S, ret, "A");
	show_paths(F, S, ret, "R");
	show_paths(F, S, ret, "W");
}

static void
show_symlink(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "L"); /* XXX same as link */
}

static void
show_unlink(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "D");
}

static void
show_rename(struct filemon *F, const struct filemon_state *S,
    const struct ktr_sysret *ret)
{
	show_paths(F, S, ret, "M");
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_chdir(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 1, &show_chdir);
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_execve(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 1, &show_execve);
}

static struct filemon_state *
filemon_sys_exit(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	const register_t *args = (const void *)&call[1];
	int status = (int)args[0];

	if (F->out != NULL) {
		fprintf(F->out, "X %jd %d\n", (intmax_t)key->pid, status);
		if (key->pid == F->child) {
			fprintf(F->out, "# Bye bye\n");
			F->child = 0;
		}
	}
	return NULL;
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_fork(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 0, &show_fork);
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_link(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 2, &show_link);
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_open(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	const register_t *args = (const void *)&call[1];
	int flags;

	if (call->ktr_argsize < 2)
		return NULL;
	flags = (int)args[1];

	if ((flags & O_RDWR) == O_RDWR)
		return syscall_enter(key, call, 1, &show_open_readwrite);
	else if ((flags & O_WRONLY) == O_WRONLY)
		return syscall_enter(key, call, 1, &show_open_write);
	else if ((flags & O_RDONLY) == O_RDONLY)
		return syscall_enter(key, call, 1, &show_open_read);
	else
		return NULL;	/* XXX Do we care if no read or write?  */
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_openat(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	const register_t *args = (const void *)&call[1];
	int flags, fd;

	if (call->ktr_argsize < 3)
		return NULL;
	fd = (int)args[0];
	flags = (int)args[2];

	if (fd == AT_CWD) {
		if ((flags & O_RDWR) == O_RDWR)
			return syscall_enter(key, call, 1,
			    &show_open_readwrite);
		else if ((flags & O_WRONLY) == O_WRONLY)
			return syscall_enter(key, call, 1, &show_open_write);
		else if ((flags & O_RDONLY) == O_RDONLY)
			return syscall_enter(key, call, 1, &show_open_read);
		else
			return NULL;
	} else {
		if ((flags & O_RDWR) == O_RDWR)
			return syscall_enter(key, call, 1,
			    &show_openat_readwrite);
		else if ((flags & O_WRONLY) == O_WRONLY)
			return syscall_enter(key, call, 1, &show_openat_write);
		else if ((flags & O_RDONLY) == O_RDONLY)
			return syscall_enter(key, call, 1, &show_openat_read);
		else
			return NULL;
	}
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_symlink(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 2, &show_symlink);
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_unlink(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 1, &show_unlink);
}

/*ARGSUSED*/
static struct filemon_state *
filemon_sys_rename(struct filemon *F, const struct filemon_key *key,
    const struct ktr_syscall *call)
{
	return syscall_enter(key, call, 2, &show_rename);
}
