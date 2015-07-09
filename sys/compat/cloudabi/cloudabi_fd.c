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

#include <compat/cloudabi/cloudabi_proto.h>

int
cloudabi_sys_fd_close(struct thread *td, struct cloudabi_sys_fd_close_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_create1(struct thread *td,
    struct cloudabi_sys_fd_create1_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_create2(struct thread *td,
    struct cloudabi_sys_fd_create2_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_datasync(struct thread *td,
    struct cloudabi_sys_fd_datasync_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_dup(struct thread *td, struct cloudabi_sys_fd_dup_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_replace(struct thread *td,
    struct cloudabi_sys_fd_replace_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_fd_seek(struct thread *td, struct cloudabi_sys_fd_seek_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
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

	/* Not implemented. */
	return (ENOSYS);
}
