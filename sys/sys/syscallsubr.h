/*
 * Copyright (c) 2002 Ian Dowse.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_SYSCALLSUBR_H_
#define _SYS_SYSCALLSUBR_H_

#include <sys/signal.h>
#include <sys/uio.h>

int	kern___getcwd(struct thread *td, u_char *buf, enum uio_seg bufseg,
	    u_int buflen);
int	kern_access(struct thread *td, char *path, enum uio_seg pathseg,
	    int flags);
int	kern_chdir(struct thread *td, char *path, enum uio_seg pathseg);
int	kern_chmod(struct thread *td, char *path, enum uio_seg pathseg,
	    int mode);
int	kern_chown(struct thread *td, char *path, enum uio_seg pathseg, int uid,
	    int gid);
int	kern_fcntl(struct thread *td, int fd, int cmd, intptr_t arg);
int	kern_futimes(struct thread *td, int fd, struct timeval *tptr,
	    enum uio_seg tptrseg);
int	kern_lchown(struct thread *td, char *path, enum uio_seg pathseg,
	    int uid, int gid);
int	kern_link(struct thread *td, char *path, char *link,
	    enum uio_seg segflg);
int	kern_lutimes(struct thread *td, char *path, enum uio_seg pathseg,
	    struct timeval *tptr, enum uio_seg tptrseg);
int	kern_mkdir(struct thread *td, char *path, enum uio_seg segflg,
	    int mode);
int	kern_mkfifo(struct thread *td, char *path, enum uio_seg pathseg,
	    int mode);
int	kern_mknod(struct thread *td, char *path, enum uio_seg pathseg,
	    int mode, int dev);
int	kern_open(struct thread *td, char *path, enum uio_seg pathseg,
	    int flags, int mode);
int	kern_readlink(struct thread *td, char *path, enum uio_seg pathseg,
	    char *buf, enum uio_seg bufseg, int count);
int	kern_rename(struct thread *td, char *from, char *to,
	    enum uio_seg pathseg);
int	kern_rmdir(struct thread *td, char *path, enum uio_seg pathseg);
int	kern_select(struct thread *td, int nd, fd_set *fd_in, fd_set *fd_ou,
	    fd_set *fd_ex, struct timeval *tvp);
int	kern_sigaction(struct thread *td, int sig, struct sigaction *act,
	    struct sigaction *oact, int old);
int	kern_sigaltstack(struct thread *td, stack_t *ss, stack_t *oss);
int	kern_sigsuspend(struct thread *td, sigset_t mask);
int	kern_symlink(struct thread *td, char *path, char *link,
	    enum uio_seg segflg);
int	kern_truncate(struct thread *td, char *path, enum uio_seg pathseg,
	    off_t length);
int	kern_unlink(struct thread *td, char *path, enum uio_seg pathseg);
int	kern_utimes(struct thread *td, char *path, enum uio_seg pathseg,
	    struct timeval *tptr, enum uio_seg tptrseg);

#endif /* !_SYS_SYSCALLSUBR_H_ */
