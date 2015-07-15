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

#include <sys/socket.h>
#include <sys/sysproto.h>

#include <compat/cloudabi/cloudabi_proto.h>
#include <compat/cloudabi/cloudabi_syscalldefs.h>

int
cloudabi_sys_sock_accept(struct thread *td,
    struct cloudabi_sys_sock_accept_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_sock_bind(struct thread *td,
    struct cloudabi_sys_sock_bind_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_sock_connect(struct thread *td,
    struct cloudabi_sys_sock_connect_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_sock_listen(struct thread *td,
    struct cloudabi_sys_sock_listen_args *uap)
{
	struct listen_args listen_args = {
		.s = uap->s,
		.backlog = uap->backlog,
	};

	return (sys_listen(td, &listen_args));
}

int
cloudabi_sys_sock_shutdown(struct thread *td,
    struct cloudabi_sys_sock_shutdown_args *uap)
{
	struct shutdown_args shutdown_args = {
		.s = uap->fd,
	};

	switch (uap->how) {
	case CLOUDABI_SHUT_RD:
		shutdown_args.how = SHUT_RD;
		break;
	case CLOUDABI_SHUT_WR:
		shutdown_args.how = SHUT_WR;
		break;
	case CLOUDABI_SHUT_RD | CLOUDABI_SHUT_WR:
		shutdown_args.how = SHUT_RDWR;
		break;
	default:
		return (EINVAL);
	}

	return (sys_shutdown(td, &shutdown_args));
}

int
cloudabi_sys_sock_stat_get(struct thread *td,
    struct cloudabi_sys_sock_stat_get_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}
