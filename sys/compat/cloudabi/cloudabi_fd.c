/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
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

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>

#include <compat/cloudabi/cloudabi_proto.h>

int
cloudabi_sys_fd_close(struct thread *td, struct cloudabi_sys_fd_close_args *uap)
{

	return (kern_close(td, uap->fd));
}

int
cloudabi_sys_fd_create1(struct thread *td,
    struct cloudabi_sys_fd_create1_args *uap)
{
	struct socket_args socket_args = {
		.domain = AF_UNIX,
	};

	switch (uap->type) {
	case CLOUDABI_FILETYPE_SOCKET_DGRAM:
		socket_args.type = SOCK_DGRAM;
		return (sys_socket(td, &socket_args));
	case CLOUDABI_FILETYPE_SOCKET_SEQPACKET:
		socket_args.type = SOCK_SEQPACKET;
		return (sys_socket(td, &socket_args));
	case CLOUDABI_FILETYPE_SOCKET_STREAM:
		socket_args.type = SOCK_STREAM;
		return (sys_socket(td, &socket_args));
	default:
		return (EINVAL);
	}
}

int
cloudabi_sys_fd_create2(struct thread *td,
    struct cloudabi_sys_fd_create2_args *uap)
{
	int fds[2];
	int error;

	switch (uap->type) {
	case CLOUDABI_FILETYPE_SOCKET_DGRAM:
		error = kern_socketpair(td, AF_UNIX, SOCK_DGRAM, 0, fds);
		break;
	case CLOUDABI_FILETYPE_SOCKET_SEQPACKET:
		error = kern_socketpair(td, AF_UNIX, SOCK_SEQPACKET, 0, fds);
		break;
	case CLOUDABI_FILETYPE_SOCKET_STREAM:
		error = kern_socketpair(td, AF_UNIX, SOCK_STREAM, 0, fds);
		break;
	default:
		return (EINVAL);
	}

	if (error == 0) {
		td->td_retval[0] = fds[0];
		td->td_retval[1] = fds[1];
	}
	return (0);
}

int
cloudabi_sys_fd_datasync(struct thread *td,
    struct cloudabi_sys_fd_datasync_args *uap)
{
	struct fsync_args fsync_args = {
		.fd = uap->fd
	};

	/* Call into fsync(), as FreeBSD lacks fdatasync(). */
	return (sys_fsync(td, &fsync_args));
}

int
cloudabi_sys_fd_dup(struct thread *td, struct cloudabi_sys_fd_dup_args *uap)
{

	return (kern_dup(td, FDDUP_NORMAL, 0, uap->from, 0));
}

int
cloudabi_sys_fd_replace(struct thread *td,
    struct cloudabi_sys_fd_replace_args *uap)
{
	int error;

	/*
	 * CloudABI's equivalent to dup2(). CloudABI processes should
	 * not depend on hardcoded file descriptor layouts, but simply
	 * use the file descriptor numbers that are allocated by the
	 * kernel. Duplicating file descriptors to arbitrary numbers
	 * should not be done.
	 *
	 * Invoke kern_dup() with FDDUP_MUSTREPLACE, so that we return
	 * EBADF when duplicating to a nonexistent file descriptor. Also
	 * clear the return value, as this system call yields no return
	 * value.
	 */
	error = kern_dup(td, FDDUP_MUSTREPLACE, 0, uap->from, uap->to);
	td->td_retval[0] = 0;
	return (error);
}

int
cloudabi_sys_fd_seek(struct thread *td, struct cloudabi_sys_fd_seek_args *uap)
{
	struct lseek_args lseek_args = {
		.fd	= uap->fd,
		.offset	= uap->offset
	};

	switch (uap->whence) {
	case CLOUDABI_WHENCE_CUR:
		lseek_args.whence = SEEK_CUR;
		break;
	case CLOUDABI_WHENCE_END:
		lseek_args.whence = SEEK_END;
		break;
	case CLOUDABI_WHENCE_SET:
		lseek_args.whence = SEEK_SET;
		break;
	default:
		return (EINVAL);
	}

	return (sys_lseek(td, &lseek_args));
}

int
cloudabi_sys_fd_stat_get(struct thread *td,
    struct cloudabi_sys_fd_stat_get_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_stat_put(struct thread *td,
    struct cloudabi_sys_fd_stat_put_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_sync(struct thread *td, struct cloudabi_sys_fd_sync_args *uap)
{
	struct fsync_args fsync_args = {
		.fd = uap->fd
	};

	return (sys_fsync(td, &fsync_args));
}
