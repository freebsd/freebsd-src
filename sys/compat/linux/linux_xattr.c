/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Dmitry Chagin <dchagin@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/extattr.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_util.h>

#define	LINUX_XATTR_SIZE_MAX	65536
#define	LINUX_XATTR_LIST_MAX	65536
#define	LINUX_XATTR_NAME_MAX	255

#define	LINUX_XATTR_CREATE	0x1
#define	LINUX_XATTR_REPLACE	0x2
#define	LINUX_XATTR_FLAGS	LINUX_XATTR_CREATE|LINUX_XATTR_REPLACE

struct listxattr_args {
	int		fd;
	const char	*path;
	char		*list;
	l_size_t	size;
	int		follow;
};

struct setxattr_args {
	int		fd;
	const char	*path;
	const char	*name;
	void 		*value;
	l_size_t	size;
	l_int		flags;
	int		follow;
};

static char *extattr_namespace_names[] = EXTATTR_NAMESPACE_NAMES;


static int
xatrr_to_extattr(const char *uattrname, int *attrnamespace, char *attrname)
{
	char uname[LINUX_XATTR_NAME_MAX + 1], *dot;
	size_t len, cplen;
	int error;

	error = copyinstr(uattrname, uname, sizeof(uname), &cplen);
	if (error != 0)
		return (error);
	if (cplen == sizeof(uname))
		return (ERANGE);
	dot = strchr(uname, '.');
	if (dot == NULL)
		return (ENOTSUP);
	*dot = '\0';
	for (*attrnamespace = EXTATTR_NAMESPACE_USER;
	    *attrnamespace < nitems(extattr_namespace_names);
	    (*attrnamespace)++) {
		if (bcmp(uname, extattr_namespace_names[*attrnamespace],
		    dot - uname + 1) == 0) {
			dot++;
			len = strlen(dot) + 1;
			bcopy(dot, attrname, len);
			return (0);
		}
	}
	return (ENOTSUP);
}

static int
listxattr(struct thread *td, struct listxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	char *data, *prefix, *key;
	struct uio auio;
	struct iovec aiov;
	unsigned char keylen;
	size_t sz, cnt, rs, prefixlen, pairlen;
	int attrnamespace, error;

	if (args->size != 0)
		sz = min(LINUX_XATTR_LIST_MAX, args->size);
	else
		sz = LINUX_XATTR_LIST_MAX;

	data = malloc(sz, M_LINUX, M_WAITOK);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_td = td;
	cnt = 0;
	for (attrnamespace = EXTATTR_NAMESPACE_USER;
	    attrnamespace < nitems(extattr_namespace_names);
	    attrnamespace++) {
		aiov.iov_base = data;
		aiov.iov_len = sz;
		auio.uio_resid = sz;
		auio.uio_offset = 0;

		if (args->path != NULL)
			error = kern_extattr_list_path(td, args->path,
			    attrnamespace, &auio, args->follow, UIO_USERSPACE);
		else
			error = kern_extattr_list_fd(td, args->fd,
			    attrnamespace, &auio);
		rs = sz - auio.uio_resid;
		if (error != 0 || rs == 0)
			continue;
		prefix = extattr_namespace_names[attrnamespace];
		prefixlen = strlen(prefix);
		key = data;
		while (rs > 0) {
			keylen = (unsigned char)key[0];
			pairlen = prefixlen + 1 + keylen + 1;
			if (cnt + pairlen > LINUX_XATTR_LIST_MAX) {
				error = E2BIG;
				break;
			}
			if ((args->list != NULL && cnt > args->size) ||
			    pairlen >= sizeof(attrname)) {
				error = ERANGE;
				break;
			}
			++key;
			if (args->list != NULL) {
				sprintf(attrname, "%s.%.*s", prefix, keylen, key);
				error = copyout(attrname, args->list, pairlen);
				if (error != 0)
					break;
				args->list += pairlen;
			}
			cnt += pairlen;
			key += keylen;
			rs -= (keylen + 1);
		}
	}
	if (error == 0)
		td->td_retval[0] = cnt;
	free(data, M_LINUX);
	return (error);
}

int
linux_listxattr(struct thread *td, struct linux_listxattr_args *args)
{
	struct listxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.list = args->list,
		.size = args->size,
		.follow = FOLLOW,
	};

	return (listxattr(td, &eargs));
}

int
linux_llistxattr(struct thread *td, struct linux_llistxattr_args *args)
{
	struct listxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.list = args->list,
		.size = args->size,
		.follow = NOFOLLOW,
	};

	return (listxattr(td, &eargs));
}

int
linux_flistxattr(struct thread *td, struct linux_flistxattr_args *args)
{
	struct listxattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.list = args->list,
		.size = args->size,
		.follow = 0,
	};

	return (listxattr(td, &eargs));
}

static int
linux_path_removexattr(struct thread *td, const char *upath, const char *uname,
    int follow)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	error = xatrr_to_extattr(uname, &attrnamespace, attrname);
	if (error != 0)
		return (error);

	return (kern_extattr_delete_path(td, upath, attrnamespace,
	    attrname, follow, UIO_USERSPACE));
}

int
linux_removexattr(struct thread *td, struct linux_removexattr_args *args)
{

	return (linux_path_removexattr(td, args->path, args->name,
	    FOLLOW));
}

int
linux_lremovexattr(struct thread *td, struct linux_lremovexattr_args *args)
{

	return (linux_path_removexattr(td, args->path, args->name,
	    NOFOLLOW));
}

int
linux_fremovexattr(struct thread *td, struct linux_fremovexattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	error = xatrr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		return (error);
	return (kern_extattr_delete_fd(td, args->fd, attrnamespace,
	    attrname));
}

static int
linux_path_getxattr(struct thread *td, const char *upath, const char *uname,
    void *value, l_size_t size, int follow)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	error = xatrr_to_extattr(uname, &attrnamespace, attrname);
	if (error != 0)
		return (error);

	return (kern_extattr_get_path(td, upath, attrnamespace,
	    attrname, value, size, follow, UIO_USERSPACE));
}

int
linux_getxattr(struct thread *td, struct linux_getxattr_args *args)
{

	return (linux_path_getxattr(td, args->path, args->name,
	    args->value, args->size, FOLLOW));
}

int
linux_lgetxattr(struct thread *td, struct linux_lgetxattr_args *args)
{

	return (linux_path_getxattr(td, args->path, args->name,
	    args->value, args->size, NOFOLLOW));
}

int
linux_fgetxattr(struct thread *td, struct linux_fgetxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	error = xatrr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		return (error);
	return (kern_extattr_get_fd(td, args->fd, attrnamespace,
	    attrname, args->value, args->size));
}

static int
setxattr(struct thread *td, struct setxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	int attrnamespace, error;

	if ((args->flags & ~(LINUX_XATTR_FLAGS)) != 0 ||
	    args->flags == (LINUX_XATTR_FLAGS))
		return (EINVAL);
	error = xatrr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		return (error);

	if ((args->flags & (LINUX_XATTR_FLAGS)) != 0 ) {
		if (args->path != NULL)
			error = kern_extattr_get_path(td, args->path,
			    attrnamespace, attrname, NULL, args->size,
			    args->follow, UIO_USERSPACE);
		else
			error = kern_extattr_get_fd(td, args->fd,
			    attrnamespace, attrname, NULL, args->size);
		if ((args->flags & LINUX_XATTR_CREATE) != 0) {
			if (error == 0)
				error = EEXIST;
			else if (error == ENOATTR)
				error = 0;
		}
		if (error != 0)
			goto out;
	}
	if (args->path != NULL)
		error = kern_extattr_set_path(td, args->path, attrnamespace,
		    attrname, args->value, args->size, args->follow,
		    UIO_USERSPACE);
	else
		error = kern_extattr_set_fd(td, args->fd, attrnamespace,
		    attrname, args->value, args->size);
out:
	td->td_retval[0] = 0;
	return (error);
}

int
linux_setxattr(struct thread *td, struct linux_setxattr_args *args)
{
	struct setxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.flags = args->flags,
		.follow = FOLLOW,
	};

	return (setxattr(td, &eargs));
}

int
linux_lsetxattr(struct thread *td, struct linux_lsetxattr_args *args)
{
	struct setxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.flags = args->flags,
		.follow = NOFOLLOW,
	};

	return (setxattr(td, &eargs));
}

int
linux_fsetxattr(struct thread *td, struct linux_fsetxattr_args *args)
{
	struct setxattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.flags = args->flags,
		.follow = 0,
	};

	return (setxattr(td, &eargs));
}
