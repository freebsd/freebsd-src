/*
 * Copyright (c) 2000-2004 Markus Friedl.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "includes.h"
RCSID("$OpenBSD: sftp-server.c,v 1.45 2004/02/19 21:15:04 markus Exp $");

#include "buffer.h"
#include "bufaux.h"
#include "getput.h"
#include "log.h"
#include "xmalloc.h"

#include "sftp.h"
#include "sftp-common.h"

/* helper */
#define get_int64()			buffer_get_int64(&iqueue);
#define get_int()			buffer_get_int(&iqueue);
#define get_string(lenp)		buffer_get_string(&iqueue, lenp);
#define TRACE				debug

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif

/* input and output queue */
Buffer iqueue;
Buffer oqueue;

/* Version of client */
int version;

/* portable attributes, etc. */

typedef struct Stat Stat;

struct Stat {
	char *name;
	char *long_name;
	Attrib attrib;
};

static int
errno_to_portable(int unixerrno)
{
	int ret = 0;

	switch (unixerrno) {
	case 0:
		ret = SSH2_FX_OK;
		break;
	case ENOENT:
	case ENOTDIR:
	case EBADF:
	case ELOOP:
		ret = SSH2_FX_NO_SUCH_FILE;
		break;
	case EPERM:
	case EACCES:
	case EFAULT:
		ret = SSH2_FX_PERMISSION_DENIED;
		break;
	case ENAMETOOLONG:
	case EINVAL:
		ret = SSH2_FX_BAD_MESSAGE;
		break;
	default:
		ret = SSH2_FX_FAILURE;
		break;
	}
	return ret;
}

static int
flags_from_portable(int pflags)
{
	int flags = 0;

	if ((pflags & SSH2_FXF_READ) &&
	    (pflags & SSH2_FXF_WRITE)) {
		flags = O_RDWR;
	} else if (pflags & SSH2_FXF_READ) {
		flags = O_RDONLY;
	} else if (pflags & SSH2_FXF_WRITE) {
		flags = O_WRONLY;
	}
	if (pflags & SSH2_FXF_CREAT)
		flags |= O_CREAT;
	if (pflags & SSH2_FXF_TRUNC)
		flags |= O_TRUNC;
	if (pflags & SSH2_FXF_EXCL)
		flags |= O_EXCL;
	return flags;
}

static Attrib *
get_attrib(void)
{
	return decode_attrib(&iqueue);
}

/* handle handles */

typedef struct Handle Handle;
struct Handle {
	int use;
	DIR *dirp;
	int fd;
	char *name;
};

enum {
	HANDLE_UNUSED,
	HANDLE_DIR,
	HANDLE_FILE
};

Handle	handles[100];

static void
handle_init(void)
{
	int i;

	for (i = 0; i < sizeof(handles)/sizeof(Handle); i++)
		handles[i].use = HANDLE_UNUSED;
}

static int
handle_new(int use, const char *name, int fd, DIR *dirp)
{
	int i;

	for (i = 0; i < sizeof(handles)/sizeof(Handle); i++) {
		if (handles[i].use == HANDLE_UNUSED) {
			handles[i].use = use;
			handles[i].dirp = dirp;
			handles[i].fd = fd;
			handles[i].name = xstrdup(name);
			return i;
		}
	}
	return -1;
}

static int
handle_is_ok(int i, int type)
{
	return i >= 0 && i < sizeof(handles)/sizeof(Handle) &&
	    handles[i].use == type;
}

static int
handle_to_string(int handle, char **stringp, int *hlenp)
{
	if (stringp == NULL || hlenp == NULL)
		return -1;
	*stringp = xmalloc(sizeof(int32_t));
	PUT_32BIT(*stringp, handle);
	*hlenp = sizeof(int32_t);
	return 0;
}

static int
handle_from_string(const char *handle, u_int hlen)
{
	int val;

	if (hlen != sizeof(int32_t))
		return -1;
	val = GET_32BIT(handle);
	if (handle_is_ok(val, HANDLE_FILE) ||
	    handle_is_ok(val, HANDLE_DIR))
		return val;
	return -1;
}

static char *
handle_to_name(int handle)
{
	if (handle_is_ok(handle, HANDLE_DIR)||
	    handle_is_ok(handle, HANDLE_FILE))
		return handles[handle].name;
	return NULL;
}

static DIR *
handle_to_dir(int handle)
{
	if (handle_is_ok(handle, HANDLE_DIR))
		return handles[handle].dirp;
	return NULL;
}

static int
handle_to_fd(int handle)
{
	if (handle_is_ok(handle, HANDLE_FILE))
		return handles[handle].fd;
	return -1;
}

static int
handle_close(int handle)
{
	int ret = -1;

	if (handle_is_ok(handle, HANDLE_FILE)) {
		ret = close(handles[handle].fd);
		handles[handle].use = HANDLE_UNUSED;
		xfree(handles[handle].name);
	} else if (handle_is_ok(handle, HANDLE_DIR)) {
		ret = closedir(handles[handle].dirp);
		handles[handle].use = HANDLE_UNUSED;
		xfree(handles[handle].name);
	} else {
		errno = ENOENT;
	}
	return ret;
}

static int
get_handle(void)
{
	char *handle;
	int val = -1;
	u_int hlen;

	handle = get_string(&hlen);
	if (hlen < 256)
		val = handle_from_string(handle, hlen);
	xfree(handle);
	return val;
}

/* send replies */

static void
send_msg(Buffer *m)
{
	int mlen = buffer_len(m);

	buffer_put_int(&oqueue, mlen);
	buffer_append(&oqueue, buffer_ptr(m), mlen);
	buffer_consume(m, mlen);
}

static void
send_status(u_int32_t id, u_int32_t error)
{
	Buffer msg;
	const char *status_messages[] = {
		"Success",			/* SSH_FX_OK */
		"End of file",			/* SSH_FX_EOF */
		"No such file",			/* SSH_FX_NO_SUCH_FILE */
		"Permission denied",		/* SSH_FX_PERMISSION_DENIED */
		"Failure",			/* SSH_FX_FAILURE */
		"Bad message",			/* SSH_FX_BAD_MESSAGE */
		"No connection",		/* SSH_FX_NO_CONNECTION */
		"Connection lost",		/* SSH_FX_CONNECTION_LOST */
		"Operation unsupported",	/* SSH_FX_OP_UNSUPPORTED */
		"Unknown error"			/* Others */
	};

	TRACE("sent status id %u error %u", id, error);
	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_STATUS);
	buffer_put_int(&msg, id);
	buffer_put_int(&msg, error);
	if (version >= 3) {
		buffer_put_cstring(&msg,
		    status_messages[MIN(error,SSH2_FX_MAX)]);
		buffer_put_cstring(&msg, "");
	}
	send_msg(&msg);
	buffer_free(&msg);
}
static void
send_data_or_handle(char type, u_int32_t id, const char *data, int dlen)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, type);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, data, dlen);
	send_msg(&msg);
	buffer_free(&msg);
}

static void
send_data(u_int32_t id, const char *data, int dlen)
{
	TRACE("sent data id %u len %d", id, dlen);
	send_data_or_handle(SSH2_FXP_DATA, id, data, dlen);
}

static void
send_handle(u_int32_t id, int handle)
{
	char *string;
	int hlen;

	handle_to_string(handle, &string, &hlen);
	TRACE("sent handle id %u handle %d", id, handle);
	send_data_or_handle(SSH2_FXP_HANDLE, id, string, hlen);
	xfree(string);
}

static void
send_names(u_int32_t id, int count, const Stat *stats)
{
	Buffer msg;
	int i;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_NAME);
	buffer_put_int(&msg, id);
	buffer_put_int(&msg, count);
	TRACE("sent names id %u count %d", id, count);
	for (i = 0; i < count; i++) {
		buffer_put_cstring(&msg, stats[i].name);
		buffer_put_cstring(&msg, stats[i].long_name);
		encode_attrib(&msg, &stats[i].attrib);
	}
	send_msg(&msg);
	buffer_free(&msg);
}

static void
send_attrib(u_int32_t id, const Attrib *a)
{
	Buffer msg;

	TRACE("sent attrib id %u have 0x%x", id, a->flags);
	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_ATTRS);
	buffer_put_int(&msg, id);
	encode_attrib(&msg, a);
	send_msg(&msg);
	buffer_free(&msg);
}

/* parse incoming */

static void
process_init(void)
{
	Buffer msg;

	version = get_int();
	TRACE("client version %d", version);
	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_VERSION);
	buffer_put_int(&msg, SSH2_FILEXFER_VERSION);
	send_msg(&msg);
	buffer_free(&msg);
}

static void
process_open(void)
{
	u_int32_t id, pflags;
	Attrib *a;
	char *name;
	int handle, fd, flags, mode, status = SSH2_FX_FAILURE;

	id = get_int();
	name = get_string(NULL);
	pflags = get_int();		/* portable flags */
	a = get_attrib();
	flags = flags_from_portable(pflags);
	mode = (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) ? a->perm : 0666;
	TRACE("open id %u name %s flags %d mode 0%o", id, name, pflags, mode);
	fd = open(name, flags, mode);
	if (fd < 0) {
		status = errno_to_portable(errno);
	} else {
		handle = handle_new(HANDLE_FILE, name, fd, NULL);
		if (handle < 0) {
			close(fd);
		} else {
			send_handle(id, handle);
			status = SSH2_FX_OK;
		}
	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
	xfree(name);
}

static void
process_close(void)
{
	u_int32_t id;
	int handle, ret, status = SSH2_FX_FAILURE;

	id = get_int();
	handle = get_handle();
	TRACE("close id %u handle %d", id, handle);
	ret = handle_close(handle);
	status = (ret == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
}

static void
process_read(void)
{
	char buf[64*1024];
	u_int32_t id, len;
	int handle, fd, ret, status = SSH2_FX_FAILURE;
	u_int64_t off;

	id = get_int();
	handle = get_handle();
	off = get_int64();
	len = get_int();

	TRACE("read id %u handle %d off %llu len %d", id, handle,
	    (u_int64_t)off, len);
	if (len > sizeof buf) {
		len = sizeof buf;
		logit("read change len %d", len);
	}
	fd = handle_to_fd(handle);
	if (fd >= 0) {
		if (lseek(fd, off, SEEK_SET) < 0) {
			error("process_read: seek failed");
			status = errno_to_portable(errno);
		} else {
			ret = read(fd, buf, len);
			if (ret < 0) {
				status = errno_to_portable(errno);
			} else if (ret == 0) {
				status = SSH2_FX_EOF;
			} else {
				send_data(id, buf, ret);
				status = SSH2_FX_OK;
			}
		}
	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
}

static void
process_write(void)
{
	u_int32_t id;
	u_int64_t off;
	u_int len;
	int handle, fd, ret, status = SSH2_FX_FAILURE;
	char *data;

	id = get_int();
	handle = get_handle();
	off = get_int64();
	data = get_string(&len);

	TRACE("write id %u handle %d off %llu len %d", id, handle,
	    (u_int64_t)off, len);
	fd = handle_to_fd(handle);
	if (fd >= 0) {
		if (lseek(fd, off, SEEK_SET) < 0) {
			status = errno_to_portable(errno);
			error("process_write: seek failed");
		} else {
/* XXX ATOMICIO ? */
			ret = write(fd, data, len);
			if (ret == -1) {
				error("process_write: write failed");
				status = errno_to_portable(errno);
			} else if (ret == len) {
				status = SSH2_FX_OK;
			} else {
				logit("nothing at all written");
			}
		}
	}
	send_status(id, status);
	xfree(data);
}

static void
process_do_stat(int do_lstat)
{
	Attrib a;
	struct stat st;
	u_int32_t id;
	char *name;
	int ret, status = SSH2_FX_FAILURE;

	id = get_int();
	name = get_string(NULL);
	TRACE("%sstat id %u name %s", do_lstat ? "l" : "", id, name);
	ret = do_lstat ? lstat(name, &st) : stat(name, &st);
	if (ret < 0) {
		status = errno_to_portable(errno);
	} else {
		stat_to_attrib(&st, &a);
		send_attrib(id, &a);
		status = SSH2_FX_OK;
	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
	xfree(name);
}

static void
process_stat(void)
{
	process_do_stat(0);
}

static void
process_lstat(void)
{
	process_do_stat(1);
}

static void
process_fstat(void)
{
	Attrib a;
	struct stat st;
	u_int32_t id;
	int fd, ret, handle, status = SSH2_FX_FAILURE;

	id = get_int();
	handle = get_handle();
	TRACE("fstat id %u handle %d", id, handle);
	fd = handle_to_fd(handle);
	if (fd  >= 0) {
		ret = fstat(fd, &st);
		if (ret < 0) {
			status = errno_to_portable(errno);
		} else {
			stat_to_attrib(&st, &a);
			send_attrib(id, &a);
			status = SSH2_FX_OK;
		}
	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
}

static struct timeval *
attrib_to_tv(const Attrib *a)
{
	static struct timeval tv[2];

	tv[0].tv_sec = a->atime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = a->mtime;
	tv[1].tv_usec = 0;
	return tv;
}

static void
process_setstat(void)
{
	Attrib *a;
	u_int32_t id;
	char *name;
	int status = SSH2_FX_OK, ret;

	id = get_int();
	name = get_string(NULL);
	a = get_attrib();
	TRACE("setstat id %u name %s", id, name);
	if (a->flags & SSH2_FILEXFER_ATTR_SIZE) {
		ret = truncate(name, a->size);
		if (ret == -1)
			status = errno_to_portable(errno);
	}
	if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) {
		ret = chmod(name, a->perm & 0777);
		if (ret == -1)
			status = errno_to_portable(errno);
	}
	if (a->flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
		ret = utimes(name, attrib_to_tv(a));
		if (ret == -1)
			status = errno_to_portable(errno);
	}
	if (a->flags & SSH2_FILEXFER_ATTR_UIDGID) {
		ret = chown(name, a->uid, a->gid);
		if (ret == -1)
			status = errno_to_portable(errno);
	}
	send_status(id, status);
	xfree(name);
}

static void
process_fsetstat(void)
{
	Attrib *a;
	u_int32_t id;
	int handle, fd, ret;
	int status = SSH2_FX_OK;
	char *name;

	id = get_int();
	handle = get_handle();
	a = get_attrib();
	TRACE("fsetstat id %u handle %d", id, handle);
	fd = handle_to_fd(handle);
	name = handle_to_name(handle);
	if (fd < 0 || name == NULL) {
		status = SSH2_FX_FAILURE;
	} else {
		if (a->flags & SSH2_FILEXFER_ATTR_SIZE) {
			ret = ftruncate(fd, a->size);
			if (ret == -1)
				status = errno_to_portable(errno);
		}
		if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) {
#ifdef HAVE_FCHMOD
			ret = fchmod(fd, a->perm & 0777);
#else
			ret = chmod(name, a->perm & 0777);
#endif
			if (ret == -1)
				status = errno_to_portable(errno);
		}
		if (a->flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
#ifdef HAVE_FUTIMES
			ret = futimes(fd, attrib_to_tv(a));
#else
			ret = utimes(name, attrib_to_tv(a));
#endif
			if (ret == -1)
				status = errno_to_portable(errno);
		}
		if (a->flags & SSH2_FILEXFER_ATTR_UIDGID) {
#ifdef HAVE_FCHOWN
			ret = fchown(fd, a->uid, a->gid);
#else
			ret = chown(name, a->uid, a->gid);
#endif
			if (ret == -1)
				status = errno_to_portable(errno);
		}
	}
	send_status(id, status);
}

static void
process_opendir(void)
{
	DIR *dirp = NULL;
	char *path;
	int handle, status = SSH2_FX_FAILURE;
	u_int32_t id;

	id = get_int();
	path = get_string(NULL);
	TRACE("opendir id %u path %s", id, path);
	dirp = opendir(path);
	if (dirp == NULL) {
		status = errno_to_portable(errno);
	} else {
		handle = handle_new(HANDLE_DIR, path, 0, dirp);
		if (handle < 0) {
			closedir(dirp);
		} else {
			send_handle(id, handle);
			status = SSH2_FX_OK;
		}

	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
	xfree(path);
}

static void
process_readdir(void)
{
	DIR *dirp;
	struct dirent *dp;
	char *path;
	int handle;
	u_int32_t id;

	id = get_int();
	handle = get_handle();
	TRACE("readdir id %u handle %d", id, handle);
	dirp = handle_to_dir(handle);
	path = handle_to_name(handle);
	if (dirp == NULL || path == NULL) {
		send_status(id, SSH2_FX_FAILURE);
	} else {
		struct stat st;
		char pathname[1024];
		Stat *stats;
		int nstats = 10, count = 0, i;

		stats = xmalloc(nstats * sizeof(Stat));
		while ((dp = readdir(dirp)) != NULL) {
			if (count >= nstats) {
				nstats *= 2;
				stats = xrealloc(stats, nstats * sizeof(Stat));
			}
/* XXX OVERFLOW ? */
			snprintf(pathname, sizeof pathname, "%s%s%s", path,
			    strcmp(path, "/") ? "/" : "", dp->d_name);
			if (lstat(pathname, &st) < 0)
				continue;
			stat_to_attrib(&st, &(stats[count].attrib));
			stats[count].name = xstrdup(dp->d_name);
			stats[count].long_name = ls_file(dp->d_name, &st, 0);
			count++;
			/* send up to 100 entries in one message */
			/* XXX check packet size instead */
			if (count == 100)
				break;
		}
		if (count > 0) {
			send_names(id, count, stats);
			for (i = 0; i < count; i++) {
				xfree(stats[i].name);
				xfree(stats[i].long_name);
			}
		} else {
			send_status(id, SSH2_FX_EOF);
		}
		xfree(stats);
	}
}

static void
process_remove(void)
{
	char *name;
	u_int32_t id;
	int status = SSH2_FX_FAILURE;
	int ret;

	id = get_int();
	name = get_string(NULL);
	TRACE("remove id %u name %s", id, name);
	ret = unlink(name);
	status = (ret == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	xfree(name);
}

static void
process_mkdir(void)
{
	Attrib *a;
	u_int32_t id;
	char *name;
	int ret, mode, status = SSH2_FX_FAILURE;

	id = get_int();
	name = get_string(NULL);
	a = get_attrib();
	mode = (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) ?
	    a->perm & 0777 : 0777;
	TRACE("mkdir id %u name %s mode 0%o", id, name, mode);
	ret = mkdir(name, mode);
	status = (ret == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	xfree(name);
}

static void
process_rmdir(void)
{
	u_int32_t id;
	char *name;
	int ret, status;

	id = get_int();
	name = get_string(NULL);
	TRACE("rmdir id %u name %s", id, name);
	ret = rmdir(name);
	status = (ret == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	xfree(name);
}

static void
process_realpath(void)
{
	char resolvedname[MAXPATHLEN];
	u_int32_t id;
	char *path;

	id = get_int();
	path = get_string(NULL);
	if (path[0] == '\0') {
		xfree(path);
		path = xstrdup(".");
	}
	TRACE("realpath id %u path %s", id, path);
	if (realpath(path, resolvedname) == NULL) {
		send_status(id, errno_to_portable(errno));
	} else {
		Stat s;
		attrib_clear(&s.attrib);
		s.name = s.long_name = resolvedname;
		send_names(id, 1, &s);
	}
	xfree(path);
}

static void
process_rename(void)
{
	u_int32_t id;
	char *oldpath, *newpath;
	int status;
	struct stat sb;

	id = get_int();
	oldpath = get_string(NULL);
	newpath = get_string(NULL);
	TRACE("rename id %u old %s new %s", id, oldpath, newpath);
	status = SSH2_FX_FAILURE;
	if (lstat(oldpath, &sb) == -1)
		status = errno_to_portable(errno);
	else if (S_ISREG(sb.st_mode)) {
		/* Race-free rename of regular files */
		if (link(oldpath, newpath) == -1)
			status = errno_to_portable(errno);
		else if (unlink(oldpath) == -1) {
			status = errno_to_portable(errno);
			/* clean spare link */
			unlink(newpath);
		} else
			status = SSH2_FX_OK;
	} else if (stat(newpath, &sb) == -1) {
		if (rename(oldpath, newpath) == -1)
			status = errno_to_portable(errno);
		else
			status = SSH2_FX_OK;
	}
	send_status(id, status);
	xfree(oldpath);
	xfree(newpath);
}

static void
process_readlink(void)
{
	u_int32_t id;
	int len;
	char link[MAXPATHLEN];
	char *path;

	id = get_int();
	path = get_string(NULL);
	TRACE("readlink id %u path %s", id, path);
	if ((len = readlink(path, link, sizeof(link) - 1)) == -1)
		send_status(id, errno_to_portable(errno));
	else {
		Stat s;

		link[len] = '\0';
		attrib_clear(&s.attrib);
		s.name = s.long_name = link;
		send_names(id, 1, &s);
	}
	xfree(path);
}

static void
process_symlink(void)
{
	u_int32_t id;
	char *oldpath, *newpath;
	int ret, status;

	id = get_int();
	oldpath = get_string(NULL);
	newpath = get_string(NULL);
	TRACE("symlink id %u old %s new %s", id, oldpath, newpath);
	/* this will fail if 'newpath' exists */
	ret = symlink(oldpath, newpath);
	status = (ret == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	xfree(oldpath);
	xfree(newpath);
}

static void
process_extended(void)
{
	u_int32_t id;
	char *request;

	id = get_int();
	request = get_string(NULL);
	send_status(id, SSH2_FX_OP_UNSUPPORTED);		/* MUST */
	xfree(request);
}

/* stolen from ssh-agent */

static void
process(void)
{
	u_int msg_len;
	u_int buf_len;
	u_int consumed;
	u_int type;
	u_char *cp;

	buf_len = buffer_len(&iqueue);
	if (buf_len < 5)
		return;		/* Incomplete message. */
	cp = buffer_ptr(&iqueue);
	msg_len = GET_32BIT(cp);
	if (msg_len > 256 * 1024) {
		error("bad message ");
		exit(11);
	}
	if (buf_len < msg_len + 4)
		return;
	buffer_consume(&iqueue, 4);
	buf_len -= 4;
	type = buffer_get_char(&iqueue);
	switch (type) {
	case SSH2_FXP_INIT:
		process_init();
		break;
	case SSH2_FXP_OPEN:
		process_open();
		break;
	case SSH2_FXP_CLOSE:
		process_close();
		break;
	case SSH2_FXP_READ:
		process_read();
		break;
	case SSH2_FXP_WRITE:
		process_write();
		break;
	case SSH2_FXP_LSTAT:
		process_lstat();
		break;
	case SSH2_FXP_FSTAT:
		process_fstat();
		break;
	case SSH2_FXP_SETSTAT:
		process_setstat();
		break;
	case SSH2_FXP_FSETSTAT:
		process_fsetstat();
		break;
	case SSH2_FXP_OPENDIR:
		process_opendir();
		break;
	case SSH2_FXP_READDIR:
		process_readdir();
		break;
	case SSH2_FXP_REMOVE:
		process_remove();
		break;
	case SSH2_FXP_MKDIR:
		process_mkdir();
		break;
	case SSH2_FXP_RMDIR:
		process_rmdir();
		break;
	case SSH2_FXP_REALPATH:
		process_realpath();
		break;
	case SSH2_FXP_STAT:
		process_stat();
		break;
	case SSH2_FXP_RENAME:
		process_rename();
		break;
	case SSH2_FXP_READLINK:
		process_readlink();
		break;
	case SSH2_FXP_SYMLINK:
		process_symlink();
		break;
	case SSH2_FXP_EXTENDED:
		process_extended();
		break;
	default:
		error("Unknown message %d", type);
		break;
	}
	/* discard the remaining bytes from the current packet */
	if (buf_len < buffer_len(&iqueue))
		fatal("iqueue grows");
	consumed = buf_len - buffer_len(&iqueue);
	if (msg_len < consumed)
		fatal("msg_len %d < consumed %d", msg_len, consumed);
	if (msg_len > consumed)
		buffer_consume(&iqueue, msg_len - consumed);
}

int
main(int ac, char **av)
{
	fd_set *rset, *wset;
	int in, out, max;
	ssize_t len, olen, set_size;

	/* XXX should use getopt */

	__progname = ssh_get_progname(av[0]);
	handle_init();

#ifdef DEBUG_SFTP_SERVER
	log_init("sftp-server", SYSLOG_LEVEL_DEBUG1, SYSLOG_FACILITY_AUTH, 0);
#endif

	in = dup(STDIN_FILENO);
	out = dup(STDOUT_FILENO);

#ifdef HAVE_CYGWIN
	setmode(in, O_BINARY);
	setmode(out, O_BINARY);
#endif

	max = 0;
	if (in > max)
		max = in;
	if (out > max)
		max = out;

	buffer_init(&iqueue);
	buffer_init(&oqueue);

	set_size = howmany(max + 1, NFDBITS) * sizeof(fd_mask);
	rset = (fd_set *)xmalloc(set_size);
	wset = (fd_set *)xmalloc(set_size);

	for (;;) {
		memset(rset, 0, set_size);
		memset(wset, 0, set_size);

		FD_SET(in, rset);
		olen = buffer_len(&oqueue);
		if (olen > 0)
			FD_SET(out, wset);

		if (select(max+1, rset, wset, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			exit(2);
		}

		/* copy stdin to iqueue */
		if (FD_ISSET(in, rset)) {
			char buf[4*4096];
			len = read(in, buf, sizeof buf);
			if (len == 0) {
				debug("read eof");
				exit(0);
			} else if (len < 0) {
				error("read error");
				exit(1);
			} else {
				buffer_append(&iqueue, buf, len);
			}
		}
		/* send oqueue to stdout */
		if (FD_ISSET(out, wset)) {
			len = write(out, buffer_ptr(&oqueue), olen);
			if (len < 0) {
				error("write error");
				exit(1);
			} else {
				buffer_consume(&oqueue, len);
			}
		}
		/* process requests from client */
		process();
	}
}
