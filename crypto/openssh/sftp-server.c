/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "includes.h"
RCSID("$OpenBSD: sftp-server.c,v 1.6 2000/09/07 20:27:53 deraadt Exp $");

#include "ssh.h"
#include "buffer.h"
#include "bufaux.h"
#include "getput.h"
#include "xmalloc.h"

/* version */
#define	SSH_FILEXFER_VERSION		2

/* client to server */
#define	SSH_FXP_INIT			1
#define	SSH_FXP_OPEN			3
#define	SSH_FXP_CLOSE			4
#define	SSH_FXP_READ			5
#define	SSH_FXP_WRITE			6
#define	SSH_FXP_LSTAT			7
#define	SSH_FXP_FSTAT			8
#define	SSH_FXP_SETSTAT			9
#define	SSH_FXP_FSETSTAT		10
#define	SSH_FXP_OPENDIR			11
#define	SSH_FXP_READDIR			12
#define	SSH_FXP_REMOVE			13
#define	SSH_FXP_MKDIR			14
#define	SSH_FXP_RMDIR			15
#define	SSH_FXP_REALPATH		16
#define	SSH_FXP_STAT			17
#define	SSH_FXP_RENAME			18

/* server to client */
#define	SSH_FXP_VERSION			2
#define	SSH_FXP_STATUS			101
#define	SSH_FXP_HANDLE			102
#define	SSH_FXP_DATA			103
#define	SSH_FXP_NAME			104
#define	SSH_FXP_ATTRS			105

/* portable open modes */
#define	SSH_FXF_READ			0x01
#define	SSH_FXF_WRITE			0x02
#define	SSH_FXF_APPEND			0x04
#define	SSH_FXF_CREAT			0x08
#define	SSH_FXF_TRUNC			0x10
#define	SSH_FXF_EXCL			0x20

/* attributes */
#define	SSH_FXA_HAVE_SIZE		0x01
#define	SSH_FXA_HAVE_UGID		0x02
#define	SSH_FXA_HAVE_PERM		0x04
#define	SSH_FXA_HAVE_TIME		0x08

/* status messages */
#define	SSH_FX_OK			0x00
#define	SSH_FX_EOF			0x01
#define	SSH_FX_NO_SUCH_FILE		0x02
#define	SSH_FX_PERMISSION_DENIED	0x03
#define	SSH_FX_FAILURE			0x04
#define	SSH_FX_BAD_MESSAGE		0x05
#define	SSH_FX_NO_CONNECTION		0x06
#define	SSH_FX_CONNECTION_LOST		0x07


/* helper */
#define get_int()			buffer_get_int(&iqueue);
#define get_string(lenp)		buffer_get_string(&iqueue, lenp);
#define TRACE				log

/* input and output queue */
Buffer iqueue;
Buffer oqueue;

/* portable attibutes, etc. */

typedef struct Attrib Attrib;
typedef struct Stat Stat;

struct Attrib
{
	u_int32_t	flags;
	u_int32_t	size_high;
	u_int32_t	size_low;
	u_int64_t	size;
	u_int32_t	uid;
	u_int32_t	gid;
	u_int32_t	perm;
	u_int32_t	atime;
	u_int32_t	mtime;
};

struct Stat
{
	char *name;
	char *long_name;
	Attrib attrib;
};

int
errno_to_portable(int unixerrno)
{
	int ret = 0;
	switch (unixerrno) {
	case 0:
		ret = SSH_FX_OK;
		break;
	case ENOENT:
	case ENOTDIR:
	case EBADF:
	case ELOOP:
		ret = SSH_FX_NO_SUCH_FILE;
		break;
	case EPERM:
	case EACCES:
	case EFAULT:
		ret = SSH_FX_PERMISSION_DENIED;
		break;
	case ENAMETOOLONG:
	case EINVAL:
		ret = SSH_FX_BAD_MESSAGE;
		break;
	default:
		ret = SSH_FX_FAILURE;
		break;
	}
	return ret;
}

int
flags_from_portable(int pflags)
{
	int flags = 0;
	if (pflags & SSH_FXF_READ &&
	    pflags & SSH_FXF_WRITE) {
		flags = O_RDWR;
	} else if (pflags & SSH_FXF_READ) {
		flags = O_RDONLY;
	} else if (pflags & SSH_FXF_WRITE) {
		flags = O_WRONLY;
	}
	if (pflags & SSH_FXF_CREAT)
		flags |= O_CREAT;
	if (pflags & SSH_FXF_TRUNC)
		flags |= O_TRUNC;
	if (pflags & SSH_FXF_EXCL)
		flags |= O_EXCL;
	return flags;
}

void
attrib_clear(Attrib *a)
{
	a->flags = 0;
	a->size_low = 0;
	a->size_high = 0;
	a->size = 0;
	a->uid = 0;
	a->gid = 0;
	a->perm = 0;
	a->atime = 0;
	a->mtime = 0;
}

Attrib *
decode_attrib(Buffer *b)
{
	static Attrib a;
	attrib_clear(&a);
	a.flags = buffer_get_int(b);
	if (a.flags & SSH_FXA_HAVE_SIZE) {
		a.size_high = buffer_get_int(b);
		a.size_low = buffer_get_int(b);
		a.size = (((u_int64_t) a.size_high) << 32) + a.size_low;
	}
	if (a.flags & SSH_FXA_HAVE_UGID) {
		a.uid = buffer_get_int(b);
		a.gid = buffer_get_int(b);
	}
	if (a.flags & SSH_FXA_HAVE_PERM) {
		a.perm = buffer_get_int(b);
	}
	if (a.flags & SSH_FXA_HAVE_TIME) {
		a.atime = buffer_get_int(b);
		a.mtime = buffer_get_int(b);
	}
	return &a;
}

void
encode_attrib(Buffer *b, Attrib *a)
{
	buffer_put_int(b, a->flags);
	if (a->flags & SSH_FXA_HAVE_SIZE) {
		buffer_put_int(b, a->size_high);
		buffer_put_int(b, a->size_low);
	}
	if (a->flags & SSH_FXA_HAVE_UGID) {
		buffer_put_int(b, a->uid);
		buffer_put_int(b, a->gid);
	}
	if (a->flags & SSH_FXA_HAVE_PERM) {
		buffer_put_int(b, a->perm);
	}
	if (a->flags & SSH_FXA_HAVE_TIME) {
		buffer_put_int(b, a->atime);
		buffer_put_int(b, a->mtime);
	}
}

Attrib *
stat_to_attrib(struct stat *st)
{
	static Attrib a;
	attrib_clear(&a);
	a.flags = 0;
	a.flags |= SSH_FXA_HAVE_SIZE;
	a.size = st->st_size;
	a.size_low = a.size;
	a.size_high = (u_int32_t) (a.size >> 32);
	a.flags |= SSH_FXA_HAVE_UGID;
	a.uid = st->st_uid;
	a.gid = st->st_gid;
	a.flags |= SSH_FXA_HAVE_PERM;
	a.perm = st->st_mode;
	a.flags |= SSH_FXA_HAVE_TIME;
	a.atime = st->st_atime;
	a.mtime = st->st_mtime;
	return &a;
}

Attrib *
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

void
handle_init(void)
{
	int i;
	for(i = 0; i < sizeof(handles)/sizeof(Handle); i++)
		handles[i].use = HANDLE_UNUSED;
}

int
handle_new(int use, char *name, int fd, DIR *dirp)
{
	int i;
	for(i = 0; i < sizeof(handles)/sizeof(Handle); i++) {
		if (handles[i].use == HANDLE_UNUSED) {
			handles[i].use = use;
			handles[i].dirp = dirp;
			handles[i].fd = fd;
			handles[i].name = name;
			return i;
		}
	}
	return -1;
}

int
handle_is_ok(int i, int type)
{
	return i >= 0 && i < sizeof(handles)/sizeof(Handle) && handles[i].use == type;
}

int
handle_to_string(int handle, char **stringp, int *hlenp)
{
	char buf[1024];
	if (stringp == NULL || hlenp == NULL)
		return -1;
	snprintf(buf, sizeof buf, "%d", handle);
	*stringp = xstrdup(buf);
	*hlenp = strlen(*stringp);
	return 0;
}

int
handle_from_string(char *handle, u_int hlen)
{
/* XXX OVERFLOW ? */
	char *ep;
	long lval = strtol(handle, &ep, 10);
	int val = lval;
	if (*ep != '\0')
		return -1;
	if (handle_is_ok(val, HANDLE_FILE) ||
	    handle_is_ok(val, HANDLE_DIR))
		return val;
	return -1;
}

char *
handle_to_name(int handle)
{
	if (handle_is_ok(handle, HANDLE_DIR)||
	    handle_is_ok(handle, HANDLE_FILE))
		return handles[handle].name;
	return NULL;
}

DIR *
handle_to_dir(int handle)
{
	if (handle_is_ok(handle, HANDLE_DIR))
		return handles[handle].dirp;
	return NULL;
}

int
handle_to_fd(int handle)
{
	if (handle_is_ok(handle, HANDLE_FILE)) 
		return handles[handle].fd;
	return -1;
}

int
handle_close(int handle)
{
	int ret = -1;
	if (handle_is_ok(handle, HANDLE_FILE)) {
		ret = close(handles[handle].fd);
		handles[handle].use = HANDLE_UNUSED;
	} else if (handle_is_ok(handle, HANDLE_DIR)) {
		ret = closedir(handles[handle].dirp);
		handles[handle].use = HANDLE_UNUSED;
	} else {
		errno = ENOENT;
	}
	return ret;
}

int
get_handle(void)
{
	char *handle;
	int val;
	u_int hlen;
	handle = get_string(&hlen);
	val = handle_from_string(handle, hlen);
	xfree(handle);
	return val;
}

/* send replies */

void
send_msg(Buffer *m)
{
	int mlen = buffer_len(m);
	buffer_put_int(&oqueue, mlen);
	buffer_append(&oqueue, buffer_ptr(m), mlen);
	buffer_consume(m, mlen);
}

void
send_status(u_int32_t id, u_int32_t error)
{
	Buffer msg;
	TRACE("sent status id %d error %d", id, error);
	buffer_init(&msg);
	buffer_put_char(&msg, SSH_FXP_STATUS);
	buffer_put_int(&msg, id);
	buffer_put_int(&msg, error);
	send_msg(&msg);
	buffer_free(&msg);
}
void
send_data_or_handle(char type, u_int32_t id, char *data, int dlen)
{
	Buffer msg;
	buffer_init(&msg);
	buffer_put_char(&msg, type);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, data, dlen);
	send_msg(&msg);
	buffer_free(&msg);
}

void
send_data(u_int32_t id, char *data, int dlen)
{
	TRACE("sent data id %d len %d", id, dlen);
	send_data_or_handle(SSH_FXP_DATA, id, data, dlen);
}

void
send_handle(u_int32_t id, int handle)
{
	char *string;
	int hlen;
	handle_to_string(handle, &string, &hlen);
	TRACE("sent handle id %d handle %d", id, handle);
	send_data_or_handle(SSH_FXP_HANDLE, id, string, hlen);
	xfree(string);
}

void
send_names(u_int32_t id, int count, Stat *stats)
{
	Buffer msg;
	int i;
	buffer_init(&msg);
	buffer_put_char(&msg, SSH_FXP_NAME);
	buffer_put_int(&msg, id);
	buffer_put_int(&msg, count);
	TRACE("sent names id %d count %d", id, count);
	for (i = 0; i < count; i++) {
		buffer_put_cstring(&msg, stats[i].name);
		buffer_put_cstring(&msg, stats[i].long_name);
		encode_attrib(&msg, &stats[i].attrib);
	}
	send_msg(&msg);
	buffer_free(&msg);
}

void
send_attrib(u_int32_t id, Attrib *a)
{
	Buffer msg;
	TRACE("sent attrib id %d have 0x%x", id, a->flags);
	buffer_init(&msg);
	buffer_put_char(&msg, SSH_FXP_ATTRS);
	buffer_put_int(&msg, id);
	encode_attrib(&msg, a);
	send_msg(&msg);
	buffer_free(&msg);
}

/* parse incoming */

void
process_init(void)
{
	Buffer msg;
	int version = buffer_get_int(&iqueue);

	TRACE("client version %d", version);
	buffer_init(&msg);
	buffer_put_char(&msg, SSH_FXP_VERSION);
	buffer_put_int(&msg, SSH_FILEXFER_VERSION);
	send_msg(&msg);
	buffer_free(&msg);
}

void
process_open(void)
{
	u_int32_t id, pflags;
	Attrib *a;
	char *name;
	int handle, fd, flags, mode, status = SSH_FX_FAILURE;

	id = get_int();
	name = get_string(NULL);
	pflags = get_int();
	a = get_attrib();
	flags = flags_from_portable(pflags);
	mode = (a->flags & SSH_FXA_HAVE_PERM) ? a->perm : 0666;
	TRACE("open id %d name %s flags %d mode 0%o", id, name, pflags, mode);
	fd = open(name, flags, mode);
	if (fd < 0) {
		status = errno_to_portable(errno);
	} else {
		handle = handle_new(HANDLE_FILE, xstrdup(name), fd, NULL);
		if (handle < 0) {
			close(fd);
		} else {
			send_handle(id, handle);
			status = SSH_FX_OK;
		}
	}
	if (status != SSH_FX_OK)
		send_status(id, status);
	xfree(name);
}

void
process_close(void)
{
	u_int32_t id;
	int handle, ret, status = SSH_FX_FAILURE;

	id = get_int();
	handle = get_handle();
	TRACE("close id %d handle %d", id, handle);
	ret = handle_close(handle);
	status = (ret == -1) ? errno_to_portable(errno) : SSH_FX_OK;
	send_status(id, status);
}

void
process_read(void)
{
	char buf[64*1024];
	u_int32_t id, off_high, off_low, len;
	int handle, fd, ret, status = SSH_FX_FAILURE;
	u_int64_t off;

	id = get_int();
	handle = get_handle();
	off_high = get_int();
	off_low = get_int();
	len = get_int();

	off = (((u_int64_t) off_high) << 32) + off_low;
	TRACE("read id %d handle %d off %qd len %d", id, handle, off, len);
	if (len > sizeof buf) {
		len = sizeof buf;
		log("read change len %d", len);
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
				status = SSH_FX_EOF;
			} else {
				send_data(id, buf, ret);
				status = SSH_FX_OK;
			}
		}
	}
	if (status != SSH_FX_OK)
		send_status(id, status);
}

void
process_write(void)
{
	u_int32_t id, off_high, off_low;
	u_int64_t off;
	u_int len;
	int handle, fd, ret, status = SSH_FX_FAILURE;
	char *data;

	id = get_int();
	handle = get_handle();
	off_high = get_int();
	off_low = get_int();
	data = get_string(&len);

	off = (((u_int64_t) off_high) << 32) + off_low;
	TRACE("write id %d handle %d off %qd len %d", id, handle, off, len);
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
				status = SSH_FX_OK;
			} else {
				log("nothing at all written");
			}
		}
	}
	send_status(id, status);
	xfree(data);
}

void
process_do_stat(int do_lstat)
{
	Attrib *a;
	struct stat st;
	u_int32_t id;
	char *name;
	int ret, status = SSH_FX_FAILURE;

	id = get_int();
	name = get_string(NULL);
	TRACE("%sstat id %d name %s", do_lstat ? "l" : "", id, name);
	ret = do_lstat ? lstat(name, &st) : stat(name, &st);
	if (ret < 0) {
		status = errno_to_portable(errno);
	} else {
		a = stat_to_attrib(&st);
		send_attrib(id, a);
		status = SSH_FX_OK;
	}
	if (status != SSH_FX_OK)
		send_status(id, status);
	xfree(name);
}

void
process_stat(void)
{
	process_do_stat(0);
}

void
process_lstat(void)
{
	process_do_stat(1);
}

void
process_fstat(void)
{
	Attrib *a;
	struct stat st;
	u_int32_t id;
	int fd, ret, handle, status = SSH_FX_FAILURE;

	id = get_int();
	handle = get_handle();
	TRACE("fstat id %d handle %d", id, handle);
	fd = handle_to_fd(handle);
	if (fd  >= 0) {
		ret = fstat(fd, &st);
		if (ret < 0) {
			status = errno_to_portable(errno);
		} else {
			a = stat_to_attrib(&st);
			send_attrib(id, a);
			status = SSH_FX_OK;
		}
	}
	if (status != SSH_FX_OK)
		send_status(id, status);
}

struct timeval *
attrib_to_tv(Attrib *a)
{
	static struct timeval tv[2];
	tv[0].tv_sec = a->atime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = a->mtime;
	tv[1].tv_usec = 0;
	return tv;
}

void
process_setstat(void)
{
	Attrib *a;
	u_int32_t id;
	char *name;
	int ret;
	int status = SSH_FX_OK;

	id = get_int();
	name = get_string(NULL);
	a = get_attrib();
	TRACE("setstat id %d name %s", id, name);
	if (a->flags & SSH_FXA_HAVE_PERM) {
		ret = chmod(name, a->perm & 0777);
		if (ret == -1)
			status = errno_to_portable(errno);
	}
	if (a->flags & SSH_FXA_HAVE_TIME) {
		ret = utimes(name, attrib_to_tv(a));
		if (ret == -1)
			status = errno_to_portable(errno);
	}
	send_status(id, status);
	xfree(name);
}

void
process_fsetstat(void)
{
	Attrib *a;
	u_int32_t id;
	int handle, fd, ret;
	int status = SSH_FX_OK;

	id = get_int();
	handle = get_handle();
	a = get_attrib();
	TRACE("fsetstat id %d handle %d", id, handle);
	fd = handle_to_fd(handle);
	if (fd < 0) {
		status = SSH_FX_FAILURE;
	} else {
		if (a->flags & SSH_FXA_HAVE_PERM) {
			ret = fchmod(fd, a->perm & 0777);
			if (ret == -1)
				status = errno_to_portable(errno);
		}
		if (a->flags & SSH_FXA_HAVE_TIME) {
			ret = futimes(fd, attrib_to_tv(a));
			if (ret == -1)
				status = errno_to_portable(errno);
		}
	}
	send_status(id, status);
}

void
process_opendir(void)
{
	DIR *dirp = NULL;
	char *path;
	int handle, status = SSH_FX_FAILURE;
	u_int32_t id;

	id = get_int();
	path = get_string(NULL);
	TRACE("opendir id %d path %s", id, path);
	dirp = opendir(path); 
	if (dirp == NULL) {
		status = errno_to_portable(errno);
	} else {
		handle = handle_new(HANDLE_DIR, xstrdup(path), 0, dirp);
		if (handle < 0) {
			closedir(dirp);
		} else {
			send_handle(id, handle);
			status = SSH_FX_OK;
		}
		
	}
	if (status != SSH_FX_OK)
		send_status(id, status);
	xfree(path);
}

char *
ls_file(char *name, struct stat *st)
{
	char buf[1024];
	snprintf(buf, sizeof buf, "0%o %d %d %qd %d %s",
	    st->st_mode, st->st_uid, st->st_gid, (long long)st->st_size,(int) st->st_mtime,
	    name);
	return xstrdup(buf);
}

void
process_readdir(void)
{
	DIR *dirp;
	struct dirent *dp;
	char *path;
	int handle;
	u_int32_t id;

	id = get_int();
	handle = get_handle();
	TRACE("readdir id %d handle %d", id, handle);
	dirp = handle_to_dir(handle);
	path = handle_to_name(handle);
	if (dirp == NULL || path == NULL) {
		send_status(id, SSH_FX_FAILURE);
	} else {
		Attrib *a;
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
			snprintf(pathname, sizeof pathname,
			    "%s/%s", path, dp->d_name);
			if (lstat(pathname, &st) < 0)
				continue;
			a = stat_to_attrib(&st);
			stats[count].attrib = *a;
			stats[count].name = xstrdup(dp->d_name);
			stats[count].long_name = ls_file(dp->d_name, &st);
			count++;
			/* send up to 100 entries in one message */
			if (count == 100)
				break;
		}
		send_names(id, count, stats);
		for(i = 0; i < count; i++) {
			xfree(stats[i].name);
			xfree(stats[i].long_name);
		}
		xfree(stats);
	}
}

void
process_remove(void)
{
	char *name;
	u_int32_t id;
	int status = SSH_FX_FAILURE;
	int ret;

	id = get_int();
	name = get_string(NULL);
	TRACE("remove id %d name %s", id, name);
	ret = remove(name);
	status = (ret == -1) ? errno_to_portable(errno) : SSH_FX_OK;
	send_status(id, status);
	xfree(name);
}

void
process_mkdir(void)
{
	Attrib *a;
	u_int32_t id;
	char *name;
	int ret, mode, status = SSH_FX_FAILURE;

	id = get_int();
	name = get_string(NULL);
	a = get_attrib();
	mode = (a->flags & SSH_FXA_HAVE_PERM) ? a->perm & 0777 : 0777;
	TRACE("mkdir id %d name %s mode 0%o", id, name, mode);
	ret = mkdir(name, mode);
	status = (ret == -1) ? errno_to_portable(errno) : SSH_FX_OK;
	send_status(id, status);
	xfree(name);
}

void
process_rmdir(void)
{
	u_int32_t id;
	char *name;
	int ret, status;

	id = get_int();
	name = get_string(NULL);
	TRACE("rmdir id %d name %s", id, name);
	ret = rmdir(name);
	status = (ret == -1) ? errno_to_portable(errno) : SSH_FX_OK;
	send_status(id, status);
	xfree(name);
}

void
process_realpath(void)
{
	char resolvedname[MAXPATHLEN];
	u_int32_t id;
	char *path;

	id = get_int();
	path = get_string(NULL);
	TRACE("realpath id %d path %s", id, path);
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

void
process_rename(void)
{
	u_int32_t id;
	char *oldpath, *newpath;
	int ret, status;

	id = get_int();
	oldpath = get_string(NULL);
	newpath = get_string(NULL);
	TRACE("rename id %d old %s new %s", id, oldpath, newpath);
	ret = rename(oldpath, newpath);
	status = (ret == -1) ? errno_to_portable(errno) : SSH_FX_OK;
	send_status(id, status);
	xfree(oldpath);
	xfree(newpath);
}


/* stolen from ssh-agent */

void
process(void)
{
	unsigned int msg_len;
	unsigned int type;
	unsigned char *cp;

	if (buffer_len(&iqueue) < 5)
		return;		/* Incomplete message. */
	cp = (unsigned char *) buffer_ptr(&iqueue);
	msg_len = GET_32BIT(cp);
	if (msg_len > 256 * 1024) {
		error("bad message ");
		exit(11);
	}
	if (buffer_len(&iqueue) < msg_len + 4)
		return;
	buffer_consume(&iqueue, 4);
	type = buffer_get_char(&iqueue);
	switch (type) {
	case SSH_FXP_INIT:
		process_init();
		break;
	case SSH_FXP_OPEN:
		process_open();
		break;
	case SSH_FXP_CLOSE:
		process_close();
		break;
	case SSH_FXP_READ:
		process_read();
		break;
	case SSH_FXP_WRITE:
		process_write();
		break;
	case SSH_FXP_LSTAT:
		process_lstat();
		break;
	case SSH_FXP_FSTAT:
		process_fstat();
		break;
	case SSH_FXP_SETSTAT:
		process_setstat();
		break;
	case SSH_FXP_FSETSTAT:
		process_fsetstat();
		break;
	case SSH_FXP_OPENDIR:
		process_opendir();
		break;
	case SSH_FXP_READDIR:
		process_readdir();
		break;
	case SSH_FXP_REMOVE:
		process_remove();
		break;
	case SSH_FXP_MKDIR:
		process_mkdir();
		break;
	case SSH_FXP_RMDIR:
		process_rmdir();
		break;
	case SSH_FXP_REALPATH:
		process_realpath();
		break;
	case SSH_FXP_STAT:
		process_stat();
		break;
	case SSH_FXP_RENAME:
		process_rename();
		break;
	default:
		error("Unknown message %d", type);
		break;
	}
}

int
main(int ac, char **av)
{
	fd_set rset, wset;
	int in, out, max;
	ssize_t len, olen;

	handle_init();

	in = dup(STDIN_FILENO);
	out = dup(STDOUT_FILENO);

	max = 0;
	if (in > max)
		max = in;
	if (out > max)
		max = out;

	buffer_init(&iqueue);
	buffer_init(&oqueue);

	for (;;) {
		FD_ZERO(&rset);
		FD_ZERO(&wset);

		FD_SET(in, &rset);
		olen = buffer_len(&oqueue);
		if (olen > 0)
			FD_SET(out, &wset);

		if (select(max+1, &rset, &wset, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			exit(2);
		}

		/* copy stdin to iqueue */
		if (FD_ISSET(in, &rset)) {
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
		if (FD_ISSET(out, &wset)) {
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
