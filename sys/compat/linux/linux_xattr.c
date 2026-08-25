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

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/extattr.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/vnode.h>

#include <security/mac/mac_framework.h>

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
#define	LINUX_XATTR_FLAGS	(LINUX_XATTR_CREATE | LINUX_XATTR_REPLACE)

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

struct getxattr_args {
	int		fd;
	const char	*path;
	const char	*name;
	void 		*value;
	l_size_t	size;
	int		follow;
};

struct removexattr_args {
	int		fd;
	const char	*path;
	const char	*name;
	int		follow;
};

static char *extattr_namespace_names[] = EXTATTR_NAMESPACE_NAMES;


static int
error_to_xattrerror(int attrnamespace, int error)
{

	if (attrnamespace == EXTATTR_NAMESPACE_SYSTEM && error == EPERM)
		return (ENOTSUP);
	else
		return (error);
}

static int
xattr_to_extattr(const char *uattrname, int *attrnamespace, char *attrname)
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
	cap_rights_t rights;
	struct file *fp = NULL;
	struct uio auio;
	struct iovec aiov;
	unsigned char keylen;
	size_t sz, cnt, rs, prefixlen, pairlen;
	int attrnamespace, error;

	if (args->path == NULL) {
		error = getvnode(td, args->fd,
		    cap_rights_init_one(&rights, CAP_EXTATTR_LIST), &fp);
		if (error != 0)
			return (error);
	}

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
			error = kern_extattr_list_fp(td, fp,
			    attrnamespace, &auio);
		rs = sz - auio.uio_resid;
		if (error == EPERM)
			break;
		if (error != 0 || rs == 0)
			continue;
		prefix = extattr_namespace_names[attrnamespace];
		prefixlen = strlen(prefix);
		key = data;
		while (rs > 0) {
			keylen = (unsigned char)key[0];
			pairlen = prefixlen + 1 + keylen + 1;
			cnt += pairlen;
			if (cnt > LINUX_XATTR_LIST_MAX) {
				error = E2BIG;
				break;
			}
			/*
			 * If size is specified as zero, return the current size
			 * of the list of extended attribute names.
			 */
			if ((args->size > 0 && cnt > args->size) ||
			    pairlen >= sizeof(attrname)) {
				error = ERANGE;
				break;
			}
			++key;
			if (args->list != NULL && args->size > 0) {
				sprintf(attrname, "%s.%.*s", prefix, keylen, key);
				error = copyout(attrname, args->list, pairlen);
				if (error != 0)
					break;
				args->list += pairlen;
			}
			key += keylen;
			rs -= (keylen + 1);
		}
	}
	if (error == 0)
		td->td_retval[0] = cnt;
	free(data, M_LINUX);
	if (fp != NULL)
		fdrop(fp, td);
	return (error_to_xattrerror(attrnamespace, error));
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
removexattr(struct thread *td, struct removexattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	struct file *fp = NULL;
	cap_rights_t rights;
	int attrnamespace, error;

	if (args->path == NULL) {
		error = getvnode(td, args->fd,
		    cap_rights_init_one(&rights, CAP_EXTATTR_DELETE), &fp);
		if (error != 0)
			return (error);
	}

	error = xattr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		goto out_err;
	if (args->path != NULL)
		error = kern_extattr_delete_path(td, args->path, attrnamespace,
		    attrname, args->follow, UIO_USERSPACE);
	else
		error = kern_extattr_delete_fp(td, fp, attrnamespace,
		    attrname);
	if (fp != NULL)
		fdrop(fp, td);
	return (error_to_xattrerror(attrnamespace, error));
out_err:
	if (fp != NULL)
		fdrop(fp, td);
	return (error);
}

int
linux_removexattr(struct thread *td, struct linux_removexattr_args *args)
{
	struct removexattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.follow = FOLLOW,
	};

	return (removexattr(td, &eargs));
}

int
linux_lremovexattr(struct thread *td, struct linux_lremovexattr_args *args)
{
	struct removexattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.follow = NOFOLLOW,
	};

	return (removexattr(td, &eargs));
}

int
linux_fremovexattr(struct thread *td, struct linux_fremovexattr_args *args)
{
	struct removexattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.name = args->name,
		.follow = 0,
	};

	return (removexattr(td, &eargs));
}

/*-
 * Linux-specific atomic extended attribute get on a vnode.
 *
 * Probes the attribute size and reads the data under a single vnode lock,
 * preventing a TOCTOU race and returning ERANGE when the buffer is too
 * small (matching Linux getxattr(2) semantics).
 */
static int
linux_extattr_get_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct thread *td)
{
	struct uio auio;
	struct iovec aiov;
	size_t size;
	int error;

	if (nbytes > IOSIZE_MAX)
		return (EINVAL);

	vn_lock(vp, LK_SHARED | LK_RETRY);

#ifdef MAC
	error = mac_vnode_check_getextattr(td->td_ucred, vp, attrnamespace,
	    attrname);
	if (error != 0)
		goto done;
#endif

	/*
	 * Probe the attribute size first under the vnode lock;
	 */
	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, NULL,
	    &size, td->td_ucred, td);
	if (error != 0)
		goto done;

	/*
	 * The caller only wants the size, so we are done after this.
	 */
	if (data == NULL || nbytes == 0) {
		td->td_retval[0] = size;
		goto done;
	}
	/*
	 * If the buffer is too small, return ERANGE
	 * so the caller can retry (Linux getxattr semantics).
	 */
	if (size > nbytes) {
		error = ERANGE;
		goto done;
	}
	/* Buffer is large enough; read the value. */
	aiov.iov_base = data;
	aiov.iov_len = nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;
	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, &auio, NULL,
		td->td_ucred, td);
	if (error == 0)
		td->td_retval[0] = nbytes - auio.uio_resid;
done:
	VOP_UNLOCK(vp);
	return (error);
}

static int
getxattr(struct thread *td, struct getxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	struct file *fp = NULL;
	struct nameidata nd;
	struct vnode *vp;
	cap_rights_t rights;
	int attrnamespace, error;

	if (args->path == NULL) {
		error = getvnode(td, args->fd,
		    cap_rights_init_one(&rights, CAP_EXTATTR_GET), &fp);
		if (error != 0)
			return (error);
		vp = fp->f_vnode;
	} else {
		NDINIT_ATRIGHTS(&nd, LOOKUP, args->follow, UIO_USERSPACE,
		    args->path, AT_FDCWD,
		    cap_rights_init_one(&rights, CAP_EXTATTR_GET));
		error = namei(&nd);
		if (error != 0)
			return (error);
		NDFREE_PNBUF(&nd);
		vp = nd.ni_vp;
	}

	error = xattr_to_extattr(args->name, &attrnamespace, attrname);
	if (error == 0) {
		error = linux_extattr_get_vp(vp, attrnamespace, attrname,
			args->value, args->size, td);
	}

	if (fp != NULL) {
		fdrop(fp, td);
	} else {
		vrele(nd.ni_vp);
	}
	return (error == EPERM || error == EOPNOTSUPP ? ENOATTR : error);
}

int
linux_getxattr(struct thread *td, struct linux_getxattr_args *args)
{
	struct getxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.follow = FOLLOW,
	};

	return (getxattr(td, &eargs));
}

int
linux_lgetxattr(struct thread *td, struct linux_lgetxattr_args *args)
{
	struct getxattr_args eargs = {
		.fd = -1,
		.path = args->path,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.follow = NOFOLLOW,
	};

	return (getxattr(td, &eargs));
}

int
linux_fgetxattr(struct thread *td, struct linux_fgetxattr_args *args)
{
	struct getxattr_args eargs = {
		.fd = args->fd,
		.path = NULL,
		.name = args->name,
		.value = args->value,
		.size = args->size,
		.follow = 0,
	};

	return (getxattr(td, &eargs));
}

static int
setxattr(struct thread *td, struct setxattr_args *args)
{
	char attrname[LINUX_XATTR_NAME_MAX + 1];
	struct file *fp = NULL;
	cap_rights_t rights;
	int attrnamespace, error;

	if (args->path == NULL) {
		if ((args->flags & LINUX_XATTR_FLAGS) != 0)
			cap_rights_init(&rights, CAP_EXTATTR_GET, CAP_EXTATTR_SET);
		else
			cap_rights_init_one(&rights, CAP_EXTATTR_SET);
		error = getvnode(td, args->fd, &rights, &fp);
		if (error != 0)
			return (error);
	}

	if ((args->flags & ~LINUX_XATTR_FLAGS) != 0 ||
	    args->flags == LINUX_XATTR_FLAGS) {
		error = EINVAL;
		goto out_err;
	}
	error = xattr_to_extattr(args->name, &attrnamespace, attrname);
	if (error != 0)
		goto out_err;

	if ((args->flags & LINUX_XATTR_FLAGS) != 0) {
		if (args->path != NULL)
			error = kern_extattr_get_path(td, args->path,
			    attrnamespace, attrname, NULL, args->size,
			    args->follow, UIO_USERSPACE);
		else
			error = kern_extattr_get_fp(td, fp,
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
		error = kern_extattr_set_fp(td, fp, attrnamespace,
		    attrname, args->value, args->size);
out:
	if (fp != NULL)
		fdrop(fp, td);
	td->td_retval[0] = 0;
	return (error_to_xattrerror(attrnamespace, error));
out_err:
	if (fp != NULL)
		fdrop(fp, td);
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
