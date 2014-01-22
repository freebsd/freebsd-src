/* $OpenBSD: sftp-client.c,v 1.108 2013/11/08 00:39:15 djm Exp $ */
/*
 * Copyright (c) 2001-2004 Damien Miller <djm@openbsd.org>
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

/* XXX: memleaks */
/* XXX: signed vs unsigned */
/* XXX: remove all logging, only return status codes */
/* XXX: copy between two remote sites */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#include "openbsd-compat/sys-queue.h"
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/uio.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "buffer.h"
#include "log.h"
#include "atomicio.h"
#include "progressmeter.h"
#include "misc.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"

extern volatile sig_atomic_t interrupted;
extern int showprogress;

/* Minimum amount of data to read at a time */
#define MIN_READ_SIZE	512

/* Maximum depth to descend in directory trees */
#define MAX_DIR_DEPTH 64

struct sftp_conn {
	int fd_in;
	int fd_out;
	u_int transfer_buflen;
	u_int num_requests;
	u_int version;
	u_int msg_id;
#define SFTP_EXT_POSIX_RENAME	0x00000001
#define SFTP_EXT_STATVFS	0x00000002
#define SFTP_EXT_FSTATVFS	0x00000004
#define SFTP_EXT_HARDLINK	0x00000008
	u_int exts;
	u_int64_t limit_kbps;
	struct bwlimit bwlimit_in, bwlimit_out;
};

static char *
get_handle(struct sftp_conn *conn, u_int expected_id, u_int *len,
    const char *errfmt, ...) __attribute__((format(printf, 4, 5)));

/* ARGSUSED */
static int
sftpio(void *_bwlimit, size_t amount)
{
	struct bwlimit *bwlimit = (struct bwlimit *)_bwlimit;

	bandwidth_limit(bwlimit, amount);
	return 0;
}

static void
send_msg(struct sftp_conn *conn, Buffer *m)
{
	u_char mlen[4];
	struct iovec iov[2];

	if (buffer_len(m) > SFTP_MAX_MSG_LENGTH)
		fatal("Outbound message too long %u", buffer_len(m));

	/* Send length first */
	put_u32(mlen, buffer_len(m));
	iov[0].iov_base = mlen;
	iov[0].iov_len = sizeof(mlen);
	iov[1].iov_base = buffer_ptr(m);
	iov[1].iov_len = buffer_len(m);

	if (atomiciov6(writev, conn->fd_out, iov, 2,
	    conn->limit_kbps > 0 ? sftpio : NULL, &conn->bwlimit_out) !=
	    buffer_len(m) + sizeof(mlen))
		fatal("Couldn't send packet: %s", strerror(errno));

	buffer_clear(m);
}

static void
get_msg(struct sftp_conn *conn, Buffer *m)
{
	u_int msg_len;

	buffer_append_space(m, 4);
	if (atomicio6(read, conn->fd_in, buffer_ptr(m), 4,
	    conn->limit_kbps > 0 ? sftpio : NULL, &conn->bwlimit_in) != 4) {
		if (errno == EPIPE)
			fatal("Connection closed");
		else
			fatal("Couldn't read packet: %s", strerror(errno));
	}

	msg_len = buffer_get_int(m);
	if (msg_len > SFTP_MAX_MSG_LENGTH)
		fatal("Received message too long %u", msg_len);

	buffer_append_space(m, msg_len);
	if (atomicio6(read, conn->fd_in, buffer_ptr(m), msg_len,
	    conn->limit_kbps > 0 ? sftpio : NULL, &conn->bwlimit_in)
	    != msg_len) {
		if (errno == EPIPE)
			fatal("Connection closed");
		else
			fatal("Read packet: %s", strerror(errno));
	}
}

static void
send_string_request(struct sftp_conn *conn, u_int id, u_int code, char *s,
    u_int len)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, code);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, s, len);
	send_msg(conn, &msg);
	debug3("Sent message fd %d T:%u I:%u", conn->fd_out, code, id);
	buffer_free(&msg);
}

static void
send_string_attrs_request(struct sftp_conn *conn, u_int id, u_int code,
    char *s, u_int len, Attrib *a)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, code);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, s, len);
	encode_attrib(&msg, a);
	send_msg(conn, &msg);
	debug3("Sent message fd %d T:%u I:%u", conn->fd_out, code, id);
	buffer_free(&msg);
}

static u_int
get_status(struct sftp_conn *conn, u_int expected_id)
{
	Buffer msg;
	u_int type, id, status;

	buffer_init(&msg);
	get_msg(conn, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%u != %u)", id, expected_id);
	if (type != SSH2_FXP_STATUS)
		fatal("Expected SSH2_FXP_STATUS(%u) packet, got %u",
		    SSH2_FXP_STATUS, type);

	status = buffer_get_int(&msg);
	buffer_free(&msg);

	debug3("SSH2_FXP_STATUS %u", status);

	return status;
}

static char *
get_handle(struct sftp_conn *conn, u_int expected_id, u_int *len,
    const char *errfmt, ...)
{
	Buffer msg;
	u_int type, id;
	char *handle, errmsg[256];
	va_list args;
	int status;

	va_start(args, errfmt);
	if (errfmt != NULL)
		vsnprintf(errmsg, sizeof(errmsg), errfmt, args);
	va_end(args);

	buffer_init(&msg);
	get_msg(conn, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("%s: ID mismatch (%u != %u)",
		    errfmt == NULL ? __func__ : errmsg, id, expected_id);
	if (type == SSH2_FXP_STATUS) {
		status = buffer_get_int(&msg);
		if (errfmt != NULL)
			error("%s: %s", errmsg, fx2txt(status));
		buffer_free(&msg);
		return(NULL);
	} else if (type != SSH2_FXP_HANDLE)
		fatal("%s: Expected SSH2_FXP_HANDLE(%u) packet, got %u",
		    errfmt == NULL ? __func__ : errmsg, SSH2_FXP_HANDLE, type);

	handle = buffer_get_string(&msg, len);
	buffer_free(&msg);

	return(handle);
}

static Attrib *
get_decode_stat(struct sftp_conn *conn, u_int expected_id, int quiet)
{
	Buffer msg;
	u_int type, id;
	Attrib *a;

	buffer_init(&msg);
	get_msg(conn, &msg);

	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	debug3("Received stat reply T:%u I:%u", type, id);
	if (id != expected_id)
		fatal("ID mismatch (%u != %u)", id, expected_id);
	if (type == SSH2_FXP_STATUS) {
		int status = buffer_get_int(&msg);

		if (quiet)
			debug("Couldn't stat remote file: %s", fx2txt(status));
		else
			error("Couldn't stat remote file: %s", fx2txt(status));
		buffer_free(&msg);
		return(NULL);
	} else if (type != SSH2_FXP_ATTRS) {
		fatal("Expected SSH2_FXP_ATTRS(%u) packet, got %u",
		    SSH2_FXP_ATTRS, type);
	}
	a = decode_attrib(&msg);
	buffer_free(&msg);

	return(a);
}

static int
get_decode_statvfs(struct sftp_conn *conn, struct sftp_statvfs *st,
    u_int expected_id, int quiet)
{
	Buffer msg;
	u_int type, id, flag;

	buffer_init(&msg);
	get_msg(conn, &msg);

	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	debug3("Received statvfs reply T:%u I:%u", type, id);
	if (id != expected_id)
		fatal("ID mismatch (%u != %u)", id, expected_id);
	if (type == SSH2_FXP_STATUS) {
		int status = buffer_get_int(&msg);

		if (quiet)
			debug("Couldn't statvfs: %s", fx2txt(status));
		else
			error("Couldn't statvfs: %s", fx2txt(status));
		buffer_free(&msg);
		return -1;
	} else if (type != SSH2_FXP_EXTENDED_REPLY) {
		fatal("Expected SSH2_FXP_EXTENDED_REPLY(%u) packet, got %u",
		    SSH2_FXP_EXTENDED_REPLY, type);
	}

	bzero(st, sizeof(*st));
	st->f_bsize = buffer_get_int64(&msg);
	st->f_frsize = buffer_get_int64(&msg);
	st->f_blocks = buffer_get_int64(&msg);
	st->f_bfree = buffer_get_int64(&msg);
	st->f_bavail = buffer_get_int64(&msg);
	st->f_files = buffer_get_int64(&msg);
	st->f_ffree = buffer_get_int64(&msg);
	st->f_favail = buffer_get_int64(&msg);
	st->f_fsid = buffer_get_int64(&msg);
	flag = buffer_get_int64(&msg);
	st->f_namemax = buffer_get_int64(&msg);

	st->f_flag = (flag & SSH2_FXE_STATVFS_ST_RDONLY) ? ST_RDONLY : 0;
	st->f_flag |= (flag & SSH2_FXE_STATVFS_ST_NOSUID) ? ST_NOSUID : 0;

	buffer_free(&msg);

	return 0;
}

struct sftp_conn *
do_init(int fd_in, int fd_out, u_int transfer_buflen, u_int num_requests,
    u_int64_t limit_kbps)
{
	u_int type;
	Buffer msg;
	struct sftp_conn *ret;

	ret = xmalloc(sizeof(*ret));
	ret->fd_in = fd_in;
	ret->fd_out = fd_out;
	ret->transfer_buflen = transfer_buflen;
	ret->num_requests = num_requests;
	ret->exts = 0;
	ret->limit_kbps = 0;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_INIT);
	buffer_put_int(&msg, SSH2_FILEXFER_VERSION);
	send_msg(ret, &msg);

	buffer_clear(&msg);

	get_msg(ret, &msg);

	/* Expecting a VERSION reply */
	if ((type = buffer_get_char(&msg)) != SSH2_FXP_VERSION) {
		error("Invalid packet back from SSH2_FXP_INIT (type %u)",
		    type);
		buffer_free(&msg);
		return(NULL);
	}
	ret->version = buffer_get_int(&msg);

	debug2("Remote version: %u", ret->version);

	/* Check for extensions */
	while (buffer_len(&msg) > 0) {
		char *name = buffer_get_string(&msg, NULL);
		char *value = buffer_get_string(&msg, NULL);
		int known = 0;

		if (strcmp(name, "posix-rename@openssh.com") == 0 &&
		    strcmp(value, "1") == 0) {
			ret->exts |= SFTP_EXT_POSIX_RENAME;
			known = 1;
		} else if (strcmp(name, "statvfs@openssh.com") == 0 &&
		    strcmp(value, "2") == 0) {
			ret->exts |= SFTP_EXT_STATVFS;
			known = 1;
		} else if (strcmp(name, "fstatvfs@openssh.com") == 0 &&
		    strcmp(value, "2") == 0) {
			ret->exts |= SFTP_EXT_FSTATVFS;
			known = 1;
		} else if (strcmp(name, "hardlink@openssh.com") == 0 &&
		    strcmp(value, "1") == 0) {
			ret->exts |= SFTP_EXT_HARDLINK;
			known = 1;
		}
		if (known) {
			debug2("Server supports extension \"%s\" revision %s",
			    name, value);
		} else {
			debug2("Unrecognised server extension \"%s\"", name);
		}
		free(name);
		free(value);
	}

	buffer_free(&msg);

	/* Some filexfer v.0 servers don't support large packets */
	if (ret->version == 0)
		ret->transfer_buflen = MIN(ret->transfer_buflen, 20480);

	ret->limit_kbps = limit_kbps;
	if (ret->limit_kbps > 0) {
		bandwidth_limit_init(&ret->bwlimit_in, ret->limit_kbps,
		    ret->transfer_buflen);
		bandwidth_limit_init(&ret->bwlimit_out, ret->limit_kbps,
		    ret->transfer_buflen);
	}

	return ret;
}

u_int
sftp_proto_version(struct sftp_conn *conn)
{
	return conn->version;
}

int
do_close(struct sftp_conn *conn, char *handle, u_int handle_len)
{
	u_int id, status;
	Buffer msg;

	buffer_init(&msg);

	id = conn->msg_id++;
	buffer_put_char(&msg, SSH2_FXP_CLOSE);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, handle, handle_len);
	send_msg(conn, &msg);
	debug3("Sent message SSH2_FXP_CLOSE I:%u", id);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't close file: %s", fx2txt(status));

	buffer_free(&msg);

	return status;
}


static int
do_lsreaddir(struct sftp_conn *conn, char *path, int printflag,
    SFTP_DIRENT ***dir)
{
	Buffer msg;
	u_int count, type, id, handle_len, i, expected_id, ents = 0;
	char *handle;

	id = conn->msg_id++;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_OPENDIR);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, path);
	send_msg(conn, &msg);

	handle = get_handle(conn, id, &handle_len,
	    "remote readdir(\"%s\")", path);
	if (handle == NULL) {
		buffer_free(&msg);
		return -1;
	}

	if (dir) {
		ents = 0;
		*dir = xcalloc(1, sizeof(**dir));
		(*dir)[0] = NULL;
	}

	for (; !interrupted;) {
		id = expected_id = conn->msg_id++;

		debug3("Sending SSH2_FXP_READDIR I:%u", id);

		buffer_clear(&msg);
		buffer_put_char(&msg, SSH2_FXP_READDIR);
		buffer_put_int(&msg, id);
		buffer_put_string(&msg, handle, handle_len);
		send_msg(conn, &msg);

		buffer_clear(&msg);

		get_msg(conn, &msg);

		type = buffer_get_char(&msg);
		id = buffer_get_int(&msg);

		debug3("Received reply T:%u I:%u", type, id);

		if (id != expected_id)
			fatal("ID mismatch (%u != %u)", id, expected_id);

		if (type == SSH2_FXP_STATUS) {
			int status = buffer_get_int(&msg);

			debug3("Received SSH2_FXP_STATUS %d", status);

			if (status == SSH2_FX_EOF) {
				break;
			} else {
				error("Couldn't read directory: %s",
				    fx2txt(status));
				do_close(conn, handle, handle_len);
				free(handle);
				buffer_free(&msg);
				return(status);
			}
		} else if (type != SSH2_FXP_NAME)
			fatal("Expected SSH2_FXP_NAME(%u) packet, got %u",
			    SSH2_FXP_NAME, type);

		count = buffer_get_int(&msg);
		if (count == 0)
			break;
		debug3("Received %d SSH2_FXP_NAME responses", count);
		for (i = 0; i < count; i++) {
			char *filename, *longname;
			Attrib *a;

			filename = buffer_get_string(&msg, NULL);
			longname = buffer_get_string(&msg, NULL);
			a = decode_attrib(&msg);

			if (printflag)
				printf("%s\n", longname);

			/*
			 * Directory entries should never contain '/'
			 * These can be used to attack recursive ops
			 * (e.g. send '../../../../etc/passwd')
			 */
			if (strchr(filename, '/') != NULL) {
				error("Server sent suspect path \"%s\" "
				    "during readdir of \"%s\"", filename, path);
				goto next;
			}

			if (dir) {
				*dir = xrealloc(*dir, ents + 2, sizeof(**dir));
				(*dir)[ents] = xcalloc(1, sizeof(***dir));
				(*dir)[ents]->filename = xstrdup(filename);
				(*dir)[ents]->longname = xstrdup(longname);
				memcpy(&(*dir)[ents]->a, a, sizeof(*a));
				(*dir)[++ents] = NULL;
			}
 next:
			free(filename);
			free(longname);
		}
	}

	buffer_free(&msg);
	do_close(conn, handle, handle_len);
	free(handle);

	/* Don't return partial matches on interrupt */
	if (interrupted && dir != NULL && *dir != NULL) {
		free_sftp_dirents(*dir);
		*dir = xcalloc(1, sizeof(**dir));
		**dir = NULL;
	}

	return 0;
}

int
do_readdir(struct sftp_conn *conn, char *path, SFTP_DIRENT ***dir)
{
	return(do_lsreaddir(conn, path, 0, dir));
}

void free_sftp_dirents(SFTP_DIRENT **s)
{
	int i;

	for (i = 0; s[i]; i++) {
		free(s[i]->filename);
		free(s[i]->longname);
		free(s[i]);
	}
	free(s);
}

int
do_rm(struct sftp_conn *conn, char *path)
{
	u_int status, id;

	debug2("Sending SSH2_FXP_REMOVE \"%s\"", path);

	id = conn->msg_id++;
	send_string_request(conn, id, SSH2_FXP_REMOVE, path, strlen(path));
	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't delete file: %s", fx2txt(status));
	return(status);
}

int
do_mkdir(struct sftp_conn *conn, char *path, Attrib *a, int printflag)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_attrs_request(conn, id, SSH2_FXP_MKDIR, path,
	    strlen(path), a);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK && printflag)
		error("Couldn't create directory: %s", fx2txt(status));

	return(status);
}

int
do_rmdir(struct sftp_conn *conn, char *path)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_request(conn, id, SSH2_FXP_RMDIR, path,
	    strlen(path));

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't remove directory: %s", fx2txt(status));

	return(status);
}

Attrib *
do_stat(struct sftp_conn *conn, char *path, int quiet)
{
	u_int id;

	id = conn->msg_id++;

	send_string_request(conn, id,
	    conn->version == 0 ? SSH2_FXP_STAT_VERSION_0 : SSH2_FXP_STAT,
	    path, strlen(path));

	return(get_decode_stat(conn, id, quiet));
}

Attrib *
do_lstat(struct sftp_conn *conn, char *path, int quiet)
{
	u_int id;

	if (conn->version == 0) {
		if (quiet)
			debug("Server version does not support lstat operation");
		else
			logit("Server version does not support lstat operation");
		return(do_stat(conn, path, quiet));
	}

	id = conn->msg_id++;
	send_string_request(conn, id, SSH2_FXP_LSTAT, path,
	    strlen(path));

	return(get_decode_stat(conn, id, quiet));
}

#ifdef notyet
Attrib *
do_fstat(struct sftp_conn *conn, char *handle, u_int handle_len, int quiet)
{
	u_int id;

	id = conn->msg_id++;
	send_string_request(conn, id, SSH2_FXP_FSTAT, handle,
	    handle_len);

	return(get_decode_stat(conn, id, quiet));
}
#endif

int
do_setstat(struct sftp_conn *conn, char *path, Attrib *a)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_attrs_request(conn, id, SSH2_FXP_SETSTAT, path,
	    strlen(path), a);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't setstat on \"%s\": %s", path,
		    fx2txt(status));

	return(status);
}

int
do_fsetstat(struct sftp_conn *conn, char *handle, u_int handle_len,
    Attrib *a)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_attrs_request(conn, id, SSH2_FXP_FSETSTAT, handle,
	    handle_len, a);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't fsetstat: %s", fx2txt(status));

	return(status);
}

char *
do_realpath(struct sftp_conn *conn, char *path)
{
	Buffer msg;
	u_int type, expected_id, count, id;
	char *filename, *longname;
	Attrib *a;

	expected_id = id = conn->msg_id++;
	send_string_request(conn, id, SSH2_FXP_REALPATH, path,
	    strlen(path));

	buffer_init(&msg);

	get_msg(conn, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%u != %u)", id, expected_id);

	if (type == SSH2_FXP_STATUS) {
		u_int status = buffer_get_int(&msg);

		error("Couldn't canonicalise: %s", fx2txt(status));
		buffer_free(&msg);
		return NULL;
	} else if (type != SSH2_FXP_NAME)
		fatal("Expected SSH2_FXP_NAME(%u) packet, got %u",
		    SSH2_FXP_NAME, type);

	count = buffer_get_int(&msg);
	if (count != 1)
		fatal("Got multiple names (%d) from SSH_FXP_REALPATH", count);

	filename = buffer_get_string(&msg, NULL);
	longname = buffer_get_string(&msg, NULL);
	a = decode_attrib(&msg);

	debug3("SSH_FXP_REALPATH %s -> %s size %lu", path, filename,
	    (unsigned long)a->size);

	free(longname);

	buffer_free(&msg);

	return(filename);
}

int
do_rename(struct sftp_conn *conn, char *oldpath, char *newpath)
{
	Buffer msg;
	u_int status, id;

	buffer_init(&msg);

	/* Send rename request */
	id = conn->msg_id++;
	if ((conn->exts & SFTP_EXT_POSIX_RENAME)) {
		buffer_put_char(&msg, SSH2_FXP_EXTENDED);
		buffer_put_int(&msg, id);
		buffer_put_cstring(&msg, "posix-rename@openssh.com");
	} else {
		buffer_put_char(&msg, SSH2_FXP_RENAME);
		buffer_put_int(&msg, id);
	}
	buffer_put_cstring(&msg, oldpath);
	buffer_put_cstring(&msg, newpath);
	send_msg(conn, &msg);
	debug3("Sent message %s \"%s\" -> \"%s\"",
	    (conn->exts & SFTP_EXT_POSIX_RENAME) ? "posix-rename@openssh.com" :
	    "SSH2_FXP_RENAME", oldpath, newpath);
	buffer_free(&msg);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't rename file \"%s\" to \"%s\": %s", oldpath,
		    newpath, fx2txt(status));

	return(status);
}

int
do_hardlink(struct sftp_conn *conn, char *oldpath, char *newpath)
{
	Buffer msg;
	u_int status, id;

	if ((conn->exts & SFTP_EXT_HARDLINK) == 0) {
		error("Server does not support hardlink@openssh.com extension");
		return -1;
	}

	buffer_init(&msg);

	/* Send link request */
	id = conn->msg_id++;
	buffer_put_char(&msg, SSH2_FXP_EXTENDED);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, "hardlink@openssh.com");
	buffer_put_cstring(&msg, oldpath);
	buffer_put_cstring(&msg, newpath);
	send_msg(conn, &msg);
	debug3("Sent message hardlink@openssh.com \"%s\" -> \"%s\"",
	       oldpath, newpath);
	buffer_free(&msg);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't link file \"%s\" to \"%s\": %s", oldpath,
		    newpath, fx2txt(status));

	return(status);
}

int
do_symlink(struct sftp_conn *conn, char *oldpath, char *newpath)
{
	Buffer msg;
	u_int status, id;

	if (conn->version < 3) {
		error("This server does not support the symlink operation");
		return(SSH2_FX_OP_UNSUPPORTED);
	}

	buffer_init(&msg);

	/* Send symlink request */
	id = conn->msg_id++;
	buffer_put_char(&msg, SSH2_FXP_SYMLINK);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, oldpath);
	buffer_put_cstring(&msg, newpath);
	send_msg(conn, &msg);
	debug3("Sent message SSH2_FXP_SYMLINK \"%s\" -> \"%s\"", oldpath,
	    newpath);
	buffer_free(&msg);

	status = get_status(conn, id);
	if (status != SSH2_FX_OK)
		error("Couldn't symlink file \"%s\" to \"%s\": %s", oldpath,
		    newpath, fx2txt(status));

	return(status);
}

#ifdef notyet
char *
do_readlink(struct sftp_conn *conn, char *path)
{
	Buffer msg;
	u_int type, expected_id, count, id;
	char *filename, *longname;
	Attrib *a;

	expected_id = id = conn->msg_id++;
	send_string_request(conn, id, SSH2_FXP_READLINK, path, strlen(path));

	buffer_init(&msg);

	get_msg(conn, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%u != %u)", id, expected_id);

	if (type == SSH2_FXP_STATUS) {
		u_int status = buffer_get_int(&msg);

		error("Couldn't readlink: %s", fx2txt(status));
		buffer_free(&msg);
		return(NULL);
	} else if (type != SSH2_FXP_NAME)
		fatal("Expected SSH2_FXP_NAME(%u) packet, got %u",
		    SSH2_FXP_NAME, type);

	count = buffer_get_int(&msg);
	if (count != 1)
		fatal("Got multiple names (%d) from SSH_FXP_READLINK", count);

	filename = buffer_get_string(&msg, NULL);
	longname = buffer_get_string(&msg, NULL);
	a = decode_attrib(&msg);

	debug3("SSH_FXP_READLINK %s -> %s", path, filename);

	free(longname);

	buffer_free(&msg);

	return(filename);
}
#endif

int
do_statvfs(struct sftp_conn *conn, const char *path, struct sftp_statvfs *st,
    int quiet)
{
	Buffer msg;
	u_int id;

	if ((conn->exts & SFTP_EXT_STATVFS) == 0) {
		error("Server does not support statvfs@openssh.com extension");
		return -1;
	}

	id = conn->msg_id++;

	buffer_init(&msg);
	buffer_clear(&msg);
	buffer_put_char(&msg, SSH2_FXP_EXTENDED);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, "statvfs@openssh.com");
	buffer_put_cstring(&msg, path);
	send_msg(conn, &msg);
	buffer_free(&msg);

	return get_decode_statvfs(conn, st, id, quiet);
}

#ifdef notyet
int
do_fstatvfs(struct sftp_conn *conn, const char *handle, u_int handle_len,
    struct sftp_statvfs *st, int quiet)
{
	Buffer msg;
	u_int id;

	if ((conn->exts & SFTP_EXT_FSTATVFS) == 0) {
		error("Server does not support fstatvfs@openssh.com extension");
		return -1;
	}

	id = conn->msg_id++;

	buffer_init(&msg);
	buffer_clear(&msg);
	buffer_put_char(&msg, SSH2_FXP_EXTENDED);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, "fstatvfs@openssh.com");
	buffer_put_string(&msg, handle, handle_len);
	send_msg(conn, &msg);
	buffer_free(&msg);

	return get_decode_statvfs(conn, st, id, quiet);
}
#endif

static void
send_read_request(struct sftp_conn *conn, u_int id, u_int64_t offset,
    u_int len, char *handle, u_int handle_len)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_clear(&msg);
	buffer_put_char(&msg, SSH2_FXP_READ);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, handle, handle_len);
	buffer_put_int64(&msg, offset);
	buffer_put_int(&msg, len);
	send_msg(conn, &msg);
	buffer_free(&msg);
}

int
do_download(struct sftp_conn *conn, char *remote_path, char *local_path,
    Attrib *a, int pflag, int resume)
{
	Attrib junk;
	Buffer msg;
	char *handle;
	int local_fd = -1, status = 0, write_error;
	int read_error, write_errno, reordered = 0;
	u_int64_t offset = 0, size, highwater;
	u_int handle_len, mode, type, id, buflen, num_req, max_req;
	off_t progress_counter;
	struct stat st;
	struct request {
		u_int id;
		u_int len;
		u_int64_t offset;
		TAILQ_ENTRY(request) tq;
	};
	TAILQ_HEAD(reqhead, request) requests;
	struct request *req;

	TAILQ_INIT(&requests);

	if (a == NULL && (a = do_stat(conn, remote_path, 0)) == NULL)
		return -1;

	/* Do not preserve set[ug]id here, as we do not preserve ownership */
	if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		mode = a->perm & 0777;
	else
		mode = 0666;

	if ((a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) &&
	    (!S_ISREG(a->perm))) {
		error("Cannot download non-regular file: %s", remote_path);
		return(-1);
	}

	if (a->flags & SSH2_FILEXFER_ATTR_SIZE)
		size = a->size;
	else
		size = 0;

	buflen = conn->transfer_buflen;
	buffer_init(&msg);

	/* Send open request */
	id = conn->msg_id++;
	buffer_put_char(&msg, SSH2_FXP_OPEN);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, remote_path);
	buffer_put_int(&msg, SSH2_FXF_READ);
	attrib_clear(&junk); /* Send empty attributes */
	encode_attrib(&msg, &junk);
	send_msg(conn, &msg);
	debug3("Sent message SSH2_FXP_OPEN I:%u P:%s", id, remote_path);

	handle = get_handle(conn, id, &handle_len,
	    "remote open(\"%s\")", remote_path);
	if (handle == NULL) {
		buffer_free(&msg);
		return(-1);
	}

	local_fd = open(local_path, O_WRONLY | O_CREAT | (resume ? 0 : O_TRUNC),
	    mode | S_IWUSR);
	if (local_fd == -1) {
		error("Couldn't open local file \"%s\" for writing: %s",
		    local_path, strerror(errno));
		goto fail;
	}
	offset = highwater = 0;
	if (resume) {
		if (fstat(local_fd, &st) == -1) {
			error("Unable to stat local file \"%s\": %s",
			    local_path, strerror(errno));
			goto fail;
		}
		if ((size_t)st.st_size > size) {
			error("Unable to resume download of \"%s\": "
			    "local file is larger than remote", local_path);
 fail:
			do_close(conn, handle, handle_len);
			buffer_free(&msg);
			free(handle);
			return -1;
		}
		offset = highwater = st.st_size;
	}

	/* Read from remote and write to local */
	write_error = read_error = write_errno = num_req = 0;
	max_req = 1;
	progress_counter = offset;

	if (showprogress && size != 0)
		start_progress_meter(remote_path, size, &progress_counter);

	while (num_req > 0 || max_req > 0) {
		char *data;
		u_int len;

		/*
		 * Simulate EOF on interrupt: stop sending new requests and
		 * allow outstanding requests to drain gracefully
		 */
		if (interrupted) {
			if (num_req == 0) /* If we haven't started yet... */
				break;
			max_req = 0;
		}

		/* Send some more requests */
		while (num_req < max_req) {
			debug3("Request range %llu -> %llu (%d/%d)",
			    (unsigned long long)offset,
			    (unsigned long long)offset + buflen - 1,
			    num_req, max_req);
			req = xcalloc(1, sizeof(*req));
			req->id = conn->msg_id++;
			req->len = buflen;
			req->offset = offset;
			offset += buflen;
			num_req++;
			TAILQ_INSERT_TAIL(&requests, req, tq);
			send_read_request(conn, req->id, req->offset,
			    req->len, handle, handle_len);
		}

		buffer_clear(&msg);
		get_msg(conn, &msg);
		type = buffer_get_char(&msg);
		id = buffer_get_int(&msg);
		debug3("Received reply T:%u I:%u R:%d", type, id, max_req);

		/* Find the request in our queue */
		for (req = TAILQ_FIRST(&requests);
		    req != NULL && req->id != id;
		    req = TAILQ_NEXT(req, tq))
			;
		if (req == NULL)
			fatal("Unexpected reply %u", id);

		switch (type) {
		case SSH2_FXP_STATUS:
			status = buffer_get_int(&msg);
			if (status != SSH2_FX_EOF)
				read_error = 1;
			max_req = 0;
			TAILQ_REMOVE(&requests, req, tq);
			free(req);
			num_req--;
			break;
		case SSH2_FXP_DATA:
			data = buffer_get_string(&msg, &len);
			debug3("Received data %llu -> %llu",
			    (unsigned long long)req->offset,
			    (unsigned long long)req->offset + len - 1);
			if (len > req->len)
				fatal("Received more data than asked for "
				    "%u > %u", len, req->len);
			if ((lseek(local_fd, req->offset, SEEK_SET) == -1 ||
			    atomicio(vwrite, local_fd, data, len) != len) &&
			    !write_error) {
				write_errno = errno;
				write_error = 1;
				max_req = 0;
			}
			else if (!reordered && req->offset <= highwater)
				highwater = req->offset + len;
			else if (!reordered && req->offset > highwater)
				reordered = 1;
			progress_counter += len;
			free(data);

			if (len == req->len) {
				TAILQ_REMOVE(&requests, req, tq);
				free(req);
				num_req--;
			} else {
				/* Resend the request for the missing data */
				debug3("Short data block, re-requesting "
				    "%llu -> %llu (%2d)",
				    (unsigned long long)req->offset + len,
				    (unsigned long long)req->offset +
				    req->len - 1, num_req);
				req->id = conn->msg_id++;
				req->len -= len;
				req->offset += len;
				send_read_request(conn, req->id,
				    req->offset, req->len, handle, handle_len);
				/* Reduce the request size */
				if (len < buflen)
					buflen = MAX(MIN_READ_SIZE, len);
			}
			if (max_req > 0) { /* max_req = 0 iff EOF received */
				if (size > 0 && offset > size) {
					/* Only one request at a time
					 * after the expected EOF */
					debug3("Finish at %llu (%2d)",
					    (unsigned long long)offset,
					    num_req);
					max_req = 1;
				} else if (max_req <= conn->num_requests) {
					++max_req;
				}
			}
			break;
		default:
			fatal("Expected SSH2_FXP_DATA(%u) packet, got %u",
			    SSH2_FXP_DATA, type);
		}
	}

	if (showprogress && size)
		stop_progress_meter();

	/* Sanity check */
	if (TAILQ_FIRST(&requests) != NULL)
		fatal("Transfer complete, but requests still in queue");
	/* Truncate at highest contiguous point to avoid holes on interrupt */
	if (read_error || write_error || interrupted) {
		if (reordered && resume) {
			error("Unable to resume download of \"%s\": "
			    "server reordered requests", local_path);
		}
		debug("truncating at %llu", (unsigned long long)highwater);
		ftruncate(local_fd, highwater);
	}
	if (read_error) {
		error("Couldn't read from remote file \"%s\" : %s",
		    remote_path, fx2txt(status));
		do_close(conn, handle, handle_len);
	} else if (write_error) {
		error("Couldn't write to \"%s\": %s", local_path,
		    strerror(write_errno));
		status = -1;
		do_close(conn, handle, handle_len);
	} else {
		status = do_close(conn, handle, handle_len);
		if (interrupted)
			status = -1;
		/* Override umask and utimes if asked */
#ifdef HAVE_FCHMOD
		if (pflag && fchmod(local_fd, mode) == -1)
#else
		if (pflag && chmod(local_path, mode) == -1)
#endif /* HAVE_FCHMOD */
			error("Couldn't set mode on \"%s\": %s", local_path,
			    strerror(errno));
		if (pflag && (a->flags & SSH2_FILEXFER_ATTR_ACMODTIME)) {
			struct timeval tv[2];
			tv[0].tv_sec = a->atime;
			tv[1].tv_sec = a->mtime;
			tv[0].tv_usec = tv[1].tv_usec = 0;
			if (utimes(local_path, tv) == -1)
				error("Can't set times on \"%s\": %s",
				    local_path, strerror(errno));
		}
	}
	close(local_fd);
	buffer_free(&msg);
	free(handle);

	return(status);
}

static int
download_dir_internal(struct sftp_conn *conn, char *src, char *dst,
    Attrib *dirattrib, int pflag, int printflag, int depth, int resume)
{
	int i, ret = 0;
	SFTP_DIRENT **dir_entries;
	char *filename, *new_src, *new_dst;
	mode_t mode = 0777;

	if (depth >= MAX_DIR_DEPTH) {
		error("Maximum directory depth exceeded: %d levels", depth);
		return -1;
	}

	if (dirattrib == NULL &&
	    (dirattrib = do_stat(conn, src, 1)) == NULL) {
		error("Unable to stat remote directory \"%s\"", src);
		return -1;
	}
	if (!S_ISDIR(dirattrib->perm)) {
		error("\"%s\" is not a directory", src);
		return -1;
	}
	if (printflag)
		printf("Retrieving %s\n", src);

	if (dirattrib->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		mode = dirattrib->perm & 01777;
	else {
		debug("Server did not send permissions for "
		    "directory \"%s\"", dst);
	}

	if (mkdir(dst, mode) == -1 && errno != EEXIST) {
		error("mkdir %s: %s", dst, strerror(errno));
		return -1;
	}

	if (do_readdir(conn, src, &dir_entries) == -1) {
		error("%s: Failed to get directory contents", src);
		return -1;
	}

	for (i = 0; dir_entries[i] != NULL && !interrupted; i++) {
		filename = dir_entries[i]->filename;

		new_dst = path_append(dst, filename);
		new_src = path_append(src, filename);

		if (S_ISDIR(dir_entries[i]->a.perm)) {
			if (strcmp(filename, ".") == 0 ||
			    strcmp(filename, "..") == 0)
				continue;
			if (download_dir_internal(conn, new_src, new_dst,
			    &(dir_entries[i]->a), pflag, printflag,
			    depth + 1, resume) == -1)
				ret = -1;
		} else if (S_ISREG(dir_entries[i]->a.perm) ) {
			if (do_download(conn, new_src, new_dst,
			    &(dir_entries[i]->a), pflag, resume) == -1) {
				error("Download of file %s to %s failed",
				    new_src, new_dst);
				ret = -1;
			}
		} else
			logit("%s: not a regular file\n", new_src);

		free(new_dst);
		free(new_src);
	}

	if (pflag) {
		if (dirattrib->flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
			struct timeval tv[2];
			tv[0].tv_sec = dirattrib->atime;
			tv[1].tv_sec = dirattrib->mtime;
			tv[0].tv_usec = tv[1].tv_usec = 0;
			if (utimes(dst, tv) == -1)
				error("Can't set times on \"%s\": %s",
				    dst, strerror(errno));
		} else
			debug("Server did not send times for directory "
			    "\"%s\"", dst);
	}

	free_sftp_dirents(dir_entries);

	return ret;
}

int
download_dir(struct sftp_conn *conn, char *src, char *dst,
    Attrib *dirattrib, int pflag, int printflag, int resume)
{
	char *src_canon;
	int ret;

	if ((src_canon = do_realpath(conn, src)) == NULL) {
		error("Unable to canonicalise path \"%s\"", src);
		return -1;
	}

	ret = download_dir_internal(conn, src_canon, dst,
	    dirattrib, pflag, printflag, 0, resume);
	free(src_canon);
	return ret;
}

int
do_upload(struct sftp_conn *conn, char *local_path, char *remote_path,
    int pflag)
{
	int local_fd;
	int status = SSH2_FX_OK;
	u_int handle_len, id, type;
	off_t offset, progress_counter;
	char *handle, *data;
	Buffer msg;
	struct stat sb;
	Attrib a;
	u_int32_t startid;
	u_int32_t ackid;
	struct outstanding_ack {
		u_int id;
		u_int len;
		off_t offset;
		TAILQ_ENTRY(outstanding_ack) tq;
	};
	TAILQ_HEAD(ackhead, outstanding_ack) acks;
	struct outstanding_ack *ack = NULL;

	TAILQ_INIT(&acks);

	if ((local_fd = open(local_path, O_RDONLY, 0)) == -1) {
		error("Couldn't open local file \"%s\" for reading: %s",
		    local_path, strerror(errno));
		return(-1);
	}
	if (fstat(local_fd, &sb) == -1) {
		error("Couldn't fstat local file \"%s\": %s",
		    local_path, strerror(errno));
		close(local_fd);
		return(-1);
	}
	if (!S_ISREG(sb.st_mode)) {
		error("%s is not a regular file", local_path);
		close(local_fd);
		return(-1);
	}
	stat_to_attrib(&sb, &a);

	a.flags &= ~SSH2_FILEXFER_ATTR_SIZE;
	a.flags &= ~SSH2_FILEXFER_ATTR_UIDGID;
	a.perm &= 0777;
	if (!pflag)
		a.flags &= ~SSH2_FILEXFER_ATTR_ACMODTIME;

	buffer_init(&msg);

	/* Send open request */
	id = conn->msg_id++;
	buffer_put_char(&msg, SSH2_FXP_OPEN);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, remote_path);
	buffer_put_int(&msg, SSH2_FXF_WRITE|SSH2_FXF_CREAT|SSH2_FXF_TRUNC);
	encode_attrib(&msg, &a);
	send_msg(conn, &msg);
	debug3("Sent message SSH2_FXP_OPEN I:%u P:%s", id, remote_path);

	buffer_clear(&msg);

	handle = get_handle(conn, id, &handle_len,
	    "remote open(\"%s\")", remote_path);
	if (handle == NULL) {
		close(local_fd);
		buffer_free(&msg);
		return -1;
	}

	startid = ackid = id + 1;
	data = xmalloc(conn->transfer_buflen);

	/* Read from local and write to remote */
	offset = progress_counter = 0;
	if (showprogress)
		start_progress_meter(local_path, sb.st_size,
		    &progress_counter);

	for (;;) {
		int len;

		/*
		 * Can't use atomicio here because it returns 0 on EOF,
		 * thus losing the last block of the file.
		 * Simulate an EOF on interrupt, allowing ACKs from the
		 * server to drain.
		 */
		if (interrupted || status != SSH2_FX_OK)
			len = 0;
		else do
			len = read(local_fd, data, conn->transfer_buflen);
		while ((len == -1) &&
		    (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

		if (len == -1)
			fatal("Couldn't read from \"%s\": %s", local_path,
			    strerror(errno));

		if (len != 0) {
			ack = xcalloc(1, sizeof(*ack));
			ack->id = ++id;
			ack->offset = offset;
			ack->len = len;
			TAILQ_INSERT_TAIL(&acks, ack, tq);

			buffer_clear(&msg);
			buffer_put_char(&msg, SSH2_FXP_WRITE);
			buffer_put_int(&msg, ack->id);
			buffer_put_string(&msg, handle, handle_len);
			buffer_put_int64(&msg, offset);
			buffer_put_string(&msg, data, len);
			send_msg(conn, &msg);
			debug3("Sent message SSH2_FXP_WRITE I:%u O:%llu S:%u",
			    id, (unsigned long long)offset, len);
		} else if (TAILQ_FIRST(&acks) == NULL)
			break;

		if (ack == NULL)
			fatal("Unexpected ACK %u", id);

		if (id == startid || len == 0 ||
		    id - ackid >= conn->num_requests) {
			u_int r_id;

			buffer_clear(&msg);
			get_msg(conn, &msg);
			type = buffer_get_char(&msg);
			r_id = buffer_get_int(&msg);

			if (type != SSH2_FXP_STATUS)
				fatal("Expected SSH2_FXP_STATUS(%d) packet, "
				    "got %d", SSH2_FXP_STATUS, type);

			status = buffer_get_int(&msg);
			debug3("SSH2_FXP_STATUS %d", status);

			/* Find the request in our queue */
			for (ack = TAILQ_FIRST(&acks);
			    ack != NULL && ack->id != r_id;
			    ack = TAILQ_NEXT(ack, tq))
				;
			if (ack == NULL)
				fatal("Can't find request for ID %u", r_id);
			TAILQ_REMOVE(&acks, ack, tq);
			debug3("In write loop, ack for %u %u bytes at %lld",
			    ack->id, ack->len, (long long)ack->offset);
			++ackid;
			progress_counter += ack->len;
			free(ack);
		}
		offset += len;
		if (offset < 0)
			fatal("%s: offset < 0", __func__);
	}
	buffer_free(&msg);

	if (showprogress)
		stop_progress_meter();
	free(data);

	if (status != SSH2_FX_OK) {
		error("Couldn't write to remote file \"%s\": %s",
		    remote_path, fx2txt(status));
		status = -1;
	}

	if (close(local_fd) == -1) {
		error("Couldn't close local file \"%s\": %s", local_path,
		    strerror(errno));
		status = -1;
	}

	/* Override umask and utimes if asked */
	if (pflag)
		do_fsetstat(conn, handle, handle_len, &a);

	if (do_close(conn, handle, handle_len) != SSH2_FX_OK)
		status = -1;
	free(handle);

	return status;
}

static int
upload_dir_internal(struct sftp_conn *conn, char *src, char *dst,
    int pflag, int printflag, int depth)
{
	int ret = 0, status;
	DIR *dirp;
	struct dirent *dp;
	char *filename, *new_src, *new_dst;
	struct stat sb;
	Attrib a;

	if (depth >= MAX_DIR_DEPTH) {
		error("Maximum directory depth exceeded: %d levels", depth);
		return -1;
	}

	if (stat(src, &sb) == -1) {
		error("Couldn't stat directory \"%s\": %s",
		    src, strerror(errno));
		return -1;
	}
	if (!S_ISDIR(sb.st_mode)) {
		error("\"%s\" is not a directory", src);
		return -1;
	}
	if (printflag)
		printf("Entering %s\n", src);

	attrib_clear(&a);
	stat_to_attrib(&sb, &a);
	a.flags &= ~SSH2_FILEXFER_ATTR_SIZE;
	a.flags &= ~SSH2_FILEXFER_ATTR_UIDGID;
	a.perm &= 01777;
	if (!pflag)
		a.flags &= ~SSH2_FILEXFER_ATTR_ACMODTIME;

	status = do_mkdir(conn, dst, &a, 0);
	/*
	 * we lack a portable status for errno EEXIST,
	 * so if we get a SSH2_FX_FAILURE back we must check
	 * if it was created successfully.
	 */
	if (status != SSH2_FX_OK) {
		if (status != SSH2_FX_FAILURE)
			return -1;
		if (do_stat(conn, dst, 0) == NULL)
			return -1;
	}

	if ((dirp = opendir(src)) == NULL) {
		error("Failed to open dir \"%s\": %s", src, strerror(errno));
		return -1;
	}

	while (((dp = readdir(dirp)) != NULL) && !interrupted) {
		if (dp->d_ino == 0)
			continue;
		filename = dp->d_name;
		new_dst = path_append(dst, filename);
		new_src = path_append(src, filename);

		if (lstat(new_src, &sb) == -1) {
			logit("%s: lstat failed: %s", filename,
			    strerror(errno));
			ret = -1;
		} else if (S_ISDIR(sb.st_mode)) {
			if (strcmp(filename, ".") == 0 ||
			    strcmp(filename, "..") == 0)
				continue;

			if (upload_dir_internal(conn, new_src, new_dst,
			    pflag, printflag, depth + 1) == -1)
				ret = -1;
		} else if (S_ISREG(sb.st_mode)) {
			if (do_upload(conn, new_src, new_dst, pflag) == -1) {
				error("Uploading of file %s to %s failed!",
				    new_src, new_dst);
				ret = -1;
			}
		} else
			logit("%s: not a regular file\n", filename);
		free(new_dst);
		free(new_src);
	}

	do_setstat(conn, dst, &a);

	(void) closedir(dirp);
	return ret;
}

int
upload_dir(struct sftp_conn *conn, char *src, char *dst, int printflag,
    int pflag)
{
	char *dst_canon;
	int ret;

	if ((dst_canon = do_realpath(conn, dst)) == NULL) {
		error("Unable to canonicalise path \"%s\"", dst);
		return -1;
	}

	ret = upload_dir_internal(conn, src, dst_canon, pflag, printflag, 0);
	free(dst_canon);
	return ret;
}

char *
path_append(char *p1, char *p2)
{
	char *ret;
	size_t len = strlen(p1) + strlen(p2) + 2;

	ret = xmalloc(len);
	strlcpy(ret, p1, len);
	if (p1[0] != '\0' && p1[strlen(p1) - 1] != '/')
		strlcat(ret, "/", len);
	strlcat(ret, p2, len);

	return(ret);
}

