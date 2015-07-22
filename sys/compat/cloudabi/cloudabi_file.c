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
#include <sys/fcntl.h>
#include <sys/syscallsubr.h>

#include <compat/cloudabi/cloudabi_proto.h>
#include <compat/cloudabi/cloudabi_syscalldefs.h>

int
cloudabi_sys_file_advise(struct thread *td,
    struct cloudabi_sys_file_advise_args *uap)
{
	int advice;

	switch (uap->advice) {
	case CLOUDABI_ADVICE_DONTNEED:
		advice = POSIX_FADV_DONTNEED;
		break;
	case CLOUDABI_ADVICE_NOREUSE:
		advice = POSIX_FADV_NOREUSE;
		break;
	case CLOUDABI_ADVICE_NORMAL:
		advice = POSIX_FADV_NORMAL;
		break;
	case CLOUDABI_ADVICE_RANDOM:
		advice = POSIX_FADV_RANDOM;
		break;
	case CLOUDABI_ADVICE_SEQUENTIAL:
		advice = POSIX_FADV_SEQUENTIAL;
		break;
	case CLOUDABI_ADVICE_WILLNEED:
		advice = POSIX_FADV_WILLNEED;
		break;
	default:
		return (EINVAL);
	}

	return (kern_posix_fadvise(td, uap->fd, uap->offset, uap->len, advice));
}

int
cloudabi_sys_file_allocate(struct thread *td,
    struct cloudabi_sys_file_allocate_args *uap)
{

	return (kern_posix_fallocate(td, uap->fd, uap->offset, uap->len));
}

int
cloudabi_sys_file_create(struct thread *td,
    struct cloudabi_sys_file_create_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_link(struct thread *td,
    struct cloudabi_sys_file_link_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_open(struct thread *td,
    struct cloudabi_sys_file_open_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_readdir(struct thread *td,
    struct cloudabi_sys_file_readdir_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_readlink(struct thread *td,
    struct cloudabi_sys_file_readlink_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_rename(struct thread *td,
    struct cloudabi_sys_file_rename_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_stat_fget(struct thread *td,
    struct cloudabi_sys_file_stat_fget_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_stat_fput(struct thread *td,
    struct cloudabi_sys_file_stat_fput_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_stat_get(struct thread *td,
    struct cloudabi_sys_file_stat_get_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_stat_put(struct thread *td,
    struct cloudabi_sys_file_stat_put_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_symlink(struct thread *td,
    struct cloudabi_sys_file_symlink_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}

int
cloudabi_sys_file_unlink(struct thread *td,
    struct cloudabi_sys_file_unlink_args *uap)
{

	/* Not implemented. */
	return (ENOSYS);
}
