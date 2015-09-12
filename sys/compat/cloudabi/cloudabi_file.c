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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/syscallsubr.h>

#include <compat/cloudabi/cloudabi_proto.h>
#include <compat/cloudabi/cloudabi_syscalldefs.h>

static MALLOC_DEFINE(M_CLOUDABI_PATH, "cloudabipath", "CloudABI pathnames");

/*
 * Copying pathnames from userspace to kernelspace.
 *
 * Unlike most operating systems, CloudABI doesn't use null-terminated
 * pathname strings. Processes always pass pathnames to the kernel by
 * providing a base pointer and a length. This has a couple of reasons:
 *
 * - It makes it easier to use CloudABI in combination with programming
 *   languages other than C, that may use non-null terminated strings.
 * - It allows for calling system calls on individual components of the
 *   pathname without modifying the input string.
 *
 * The function below copies in pathname strings and null-terminates it.
 * It also ensure that the string itself does not contain any null
 * bytes.
 *
 * TODO(ed): Add an abstraction to vfs_lookup.c that allows us to pass
 *           in unterminated pathname strings, so we can do away with
 *           the copying.
 */

static int
copyin_path(const char *uaddr, size_t len, char **result)
{
	char *buf;
	int error;

	if (len >= PATH_MAX)
		return (ENAMETOOLONG);
	buf = malloc(len + 1, M_CLOUDABI_PATH, M_WAITOK);
	error = copyin(uaddr, buf, len);
	if (error != 0) {
		free(buf, M_CLOUDABI_PATH);
		return (error);
	}
	if (memchr(buf, '\0', len) != NULL) {
		free(buf, M_CLOUDABI_PATH);
		return (EINVAL);
	}
	buf[len] = '\0';
	*result = buf;
	return (0);
}

static void
cloudabi_freestr(char *buf)
{

	free(buf, M_CLOUDABI_PATH);
}

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
	char *path1, *path2;
	int error;

	error = copyin_path(uap->path1, uap->path1len, &path1);
	if (error != 0)
		return (error);
	error = copyin_path(uap->path2, uap->path2len, &path2);
	if (error != 0) {
		cloudabi_freestr(path1);
		return (error);
	}

	error = kern_linkat(td, uap->fd1, uap->fd2, path1, path2,
	    UIO_SYSSPACE, (uap->fd1 & CLOUDABI_LOOKUP_SYMLINK_FOLLOW) != 0 ?
	    FOLLOW : NOFOLLOW);
	cloudabi_freestr(path1);
	cloudabi_freestr(path2);
	return (error);
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
	char *path;
	int error;

	error = copyin_path(uap->path, uap->pathlen, &path);
	if (error != 0)
		return (error);

	error = kern_readlinkat(td, uap->fd, path, UIO_SYSSPACE,
	    uap->buf, UIO_USERSPACE, uap->bufsize);
	cloudabi_freestr(path);
	return (error);
}

int
cloudabi_sys_file_rename(struct thread *td,
    struct cloudabi_sys_file_rename_args *uap)
{
	char *old, *new;
	int error;

	error = copyin_path(uap->old, uap->oldlen, &old);
	if (error != 0)
		return (error);
	error = copyin_path(uap->new, uap->newlen, &new);
	if (error != 0) {
		cloudabi_freestr(old);
		return (error);
	}

	error = kern_renameat(td, uap->oldfd, old, uap->newfd, new,
	    UIO_SYSSPACE);
	cloudabi_freestr(old);
	cloudabi_freestr(new);
	return (error);
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
	char *path1, *path2;
	int error;

	error = copyin_path(uap->path1, uap->path1len, &path1);
	if (error != 0)
		return (error);
	error = copyin_path(uap->path2, uap->path2len, &path2);
	if (error != 0) {
		cloudabi_freestr(path1);
		return (error);
	}

	error = kern_symlinkat(td, path1, uap->fd, path2, UIO_SYSSPACE);
	cloudabi_freestr(path1);
	cloudabi_freestr(path2);
	return (error);
}

int
cloudabi_sys_file_unlink(struct thread *td,
    struct cloudabi_sys_file_unlink_args *uap)
{
	char *path;
	int error;

	error = copyin_path(uap->path, uap->pathlen, &path);
	if (error != 0)
		return (error);

	if (uap->flag & CLOUDABI_UNLINK_REMOVEDIR)
		error = kern_rmdirat(td, uap->fd, path, UIO_SYSSPACE);
	else
		error = kern_unlinkat(td, uap->fd, path, UIO_SYSSPACE, 0);
	cloudabi_freestr(path);
	return (error);
}
