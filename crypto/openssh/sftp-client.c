/*
 * Copyright (c) 2001,2002 Damien Miller.  All rights reserved.
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

/* XXX: memleaks */
/* XXX: signed vs unsigned */
/* XXX: remove all logging, only return status codes */
/* XXX: copy between two remote sites */

#include "includes.h"
RCSID("$OpenBSD: sftp-client.c,v 1.24 2002/02/24 16:57:19 markus Exp $");

#include <sys/queue.h>

#include "buffer.h"
#include "bufaux.h"
#include "getput.h"
#include "xmalloc.h"
#include "log.h"
#include "atomicio.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"

/* Minimum amount of data to read at at time */
#define MIN_READ_SIZE	512

struct sftp_conn {
	int fd_in;
	int fd_out;
	u_int transfer_buflen;
	u_int num_requests;
	u_int version;
	u_int msg_id;
};

static void
send_msg(int fd, Buffer *m)
{
	int mlen = buffer_len(m);
	int len;
	Buffer oqueue;

	buffer_init(&oqueue);
	buffer_put_int(&oqueue, mlen);
	buffer_append(&oqueue, buffer_ptr(m), mlen);
	buffer_consume(m, mlen);

	len = atomicio(write, fd, buffer_ptr(&oqueue), buffer_len(&oqueue));
	if (len <= 0)
		fatal("Couldn't send packet: %s", strerror(errno));

	buffer_free(&oqueue);
}

static void
get_msg(int fd, Buffer *m)
{
	u_int len, msg_len;
	unsigned char buf[4096];

	len = atomicio(read, fd, buf, 4);
	if (len == 0)
		fatal("Connection closed");
	else if (len == -1)
		fatal("Couldn't read packet: %s", strerror(errno));

	msg_len = GET_32BIT(buf);
	if (msg_len > 256 * 1024)
		fatal("Received message too long %d", msg_len);

	while (msg_len) {
		len = atomicio(read, fd, buf, MIN(msg_len, sizeof(buf)));
		if (len == 0)
			fatal("Connection closed");
		else if (len == -1)
			fatal("Couldn't read packet: %s", strerror(errno));

		msg_len -= len;
		buffer_append(m, buf, len);
	}
}

static void
send_string_request(int fd, u_int id, u_int code, char *s,
    u_int len)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, code);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, s, len);
	send_msg(fd, &msg);
	debug3("Sent message fd %d T:%d I:%d", fd, code, id);
	buffer_free(&msg);
}

static void
send_string_attrs_request(int fd, u_int id, u_int code, char *s,
    u_int len, Attrib *a)
{
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, code);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, s, len);
	encode_attrib(&msg, a);
	send_msg(fd, &msg);
	debug3("Sent message fd %d T:%d I:%d", fd, code, id);
	buffer_free(&msg);
}

static u_int
get_status(int fd, int expected_id)
{
	Buffer msg;
	u_int type, id, status;

	buffer_init(&msg);
	get_msg(fd, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%d != %d)", id, expected_id);
	if (type != SSH2_FXP_STATUS)
		fatal("Expected SSH2_FXP_STATUS(%d) packet, got %d",
		    SSH2_FXP_STATUS, type);

	status = buffer_get_int(&msg);
	buffer_free(&msg);

	debug3("SSH2_FXP_STATUS %d", status);

	return(status);
}

static char *
get_handle(int fd, u_int expected_id, u_int *len)
{
	Buffer msg;
	u_int type, id;
	char *handle;

	buffer_init(&msg);
	get_msg(fd, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%d != %d)", id, expected_id);
	if (type == SSH2_FXP_STATUS) {
		int status = buffer_get_int(&msg);

		error("Couldn't get handle: %s", fx2txt(status));
		return(NULL);
	} else if (type != SSH2_FXP_HANDLE)
		fatal("Expected SSH2_FXP_HANDLE(%d) packet, got %d",
		    SSH2_FXP_HANDLE, type);

	handle = buffer_get_string(&msg, len);
	buffer_free(&msg);

	return(handle);
}

static Attrib *
get_decode_stat(int fd, u_int expected_id, int quiet)
{
	Buffer msg;
	u_int type, id;
	Attrib *a;

	buffer_init(&msg);
	get_msg(fd, &msg);

	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	debug3("Received stat reply T:%d I:%d", type, id);
	if (id != expected_id)
		fatal("ID mismatch (%d != %d)", id, expected_id);
	if (type == SSH2_FXP_STATUS) {
		int status = buffer_get_int(&msg);

		if (quiet)
			debug("Couldn't stat remote file: %s", fx2txt(status));
		else
			error("Couldn't stat remote file: %s", fx2txt(status));
		return(NULL);
	} else if (type != SSH2_FXP_ATTRS) {
		fatal("Expected SSH2_FXP_ATTRS(%d) packet, got %d",
		    SSH2_FXP_ATTRS, type);
	}
	a = decode_attrib(&msg);
	buffer_free(&msg);

	return(a);
}

struct sftp_conn *
do_init(int fd_in, int fd_out, u_int transfer_buflen, u_int num_requests)
{
	int type, version;
	Buffer msg;
	struct sftp_conn *ret;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_INIT);
	buffer_put_int(&msg, SSH2_FILEXFER_VERSION);
	send_msg(fd_out, &msg);

	buffer_clear(&msg);

	get_msg(fd_in, &msg);

	/* Expecting a VERSION reply */
	if ((type = buffer_get_char(&msg)) != SSH2_FXP_VERSION) {
		error("Invalid packet back from SSH2_FXP_INIT (type %d)",
		    type);
		buffer_free(&msg);
		return(NULL);
	}
	version = buffer_get_int(&msg);

	debug2("Remote version: %d", version);

	/* Check for extensions */
	while (buffer_len(&msg) > 0) {
		char *name = buffer_get_string(&msg, NULL);
		char *value = buffer_get_string(&msg, NULL);

		debug2("Init extension: \"%s\"", name);
		xfree(name);
		xfree(value);
	}

	buffer_free(&msg);

	ret = xmalloc(sizeof(*ret));
	ret->fd_in = fd_in;
	ret->fd_out = fd_out;
	ret->transfer_buflen = transfer_buflen;
	ret->num_requests = num_requests;
	ret->version = version;
	ret->msg_id = 1;

	/* Some filexfer v.0 servers don't support large packets */
	if (version == 0)
		ret->transfer_buflen = MAX(ret->transfer_buflen, 20480);

	return(ret);
}

u_int
sftp_proto_version(struct sftp_conn *conn)
{
	return(conn->version);
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
	send_msg(conn->fd_out, &msg);
	debug3("Sent message SSH2_FXP_CLOSE I:%d", id);

	status = get_status(conn->fd_in, id);
	if (status != SSH2_FX_OK)
		error("Couldn't close file: %s", fx2txt(status));

	buffer_free(&msg);

	return(status);
}


static int
do_lsreaddir(struct sftp_conn *conn, char *path, int printflag,
    SFTP_DIRENT ***dir)
{
	Buffer msg;
	u_int type, id, handle_len, i, expected_id, ents = 0;
	char *handle;

	id = conn->msg_id++;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_FXP_OPENDIR);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, path);
	send_msg(conn->fd_out, &msg);

	buffer_clear(&msg);

	handle = get_handle(conn->fd_in, id, &handle_len);
	if (handle == NULL)
		return(-1);

	if (dir) {
		ents = 0;
		*dir = xmalloc(sizeof(**dir));
		(*dir)[0] = NULL;
	}

	for (;;) {
		int count;

		id = expected_id = conn->msg_id++;

		debug3("Sending SSH2_FXP_READDIR I:%d", id);

		buffer_clear(&msg);
		buffer_put_char(&msg, SSH2_FXP_READDIR);
		buffer_put_int(&msg, id);
		buffer_put_string(&msg, handle, handle_len);
		send_msg(conn->fd_out, &msg);

		buffer_clear(&msg);

		get_msg(conn->fd_in, &msg);

		type = buffer_get_char(&msg);
		id = buffer_get_int(&msg);

		debug3("Received reply T:%d I:%d", type, id);

		if (id != expected_id)
			fatal("ID mismatch (%d != %d)", id, expected_id);

		if (type == SSH2_FXP_STATUS) {
			int status = buffer_get_int(&msg);

			debug3("Received SSH2_FXP_STATUS %d", status);

			if (status == SSH2_FX_EOF) {
				break;
			} else {
				error("Couldn't read directory: %s",
				    fx2txt(status));
				do_close(conn, handle, handle_len);
				return(status);
			}
		} else if (type != SSH2_FXP_NAME)
			fatal("Expected SSH2_FXP_NAME(%d) packet, got %d",
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

			if (dir) {
				*dir = xrealloc(*dir, sizeof(**dir) *
				    (ents + 2));
				(*dir)[ents] = xmalloc(sizeof(***dir));
				(*dir)[ents]->filename = xstrdup(filename);
				(*dir)[ents]->longname = xstrdup(longname);
				memcpy(&(*dir)[ents]->a, a, sizeof(*a));
				(*dir)[++ents] = NULL;
			}

			xfree(filename);
			xfree(longname);
		}
	}

	buffer_free(&msg);
	do_close(conn, handle, handle_len);
	xfree(handle);

	return(0);
}

int
do_ls(struct sftp_conn *conn, char *path)
{
	return(do_lsreaddir(conn, path, 1, NULL));
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
		xfree(s[i]->filename);
		xfree(s[i]->longname);
		xfree(s[i]);
	}
	xfree(s);
}

int
do_rm(struct sftp_conn *conn, char *path)
{
	u_int status, id;

	debug2("Sending SSH2_FXP_REMOVE \"%s\"", path);

	id = conn->msg_id++;
	send_string_request(conn->fd_out, id, SSH2_FXP_REMOVE, path, 
	    strlen(path));
	status = get_status(conn->fd_in, id);
	if (status != SSH2_FX_OK)
		error("Couldn't delete file: %s", fx2txt(status));
	return(status);
}

int
do_mkdir(struct sftp_conn *conn, char *path, Attrib *a)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_attrs_request(conn->fd_out, id, SSH2_FXP_MKDIR, path,
	    strlen(path), a);

	status = get_status(conn->fd_in, id);
	if (status != SSH2_FX_OK)
		error("Couldn't create directory: %s", fx2txt(status));

	return(status);
}

int
do_rmdir(struct sftp_conn *conn, char *path)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_request(conn->fd_out, id, SSH2_FXP_RMDIR, path,
	    strlen(path));

	status = get_status(conn->fd_in, id);
	if (status != SSH2_FX_OK)
		error("Couldn't remove directory: %s", fx2txt(status));

	return(status);
}

Attrib *
do_stat(struct sftp_conn *conn, char *path, int quiet)
{
	u_int id;

	id = conn->msg_id++;

	send_string_request(conn->fd_out, id, 
	    conn->version == 0 ? SSH2_FXP_STAT_VERSION_0 : SSH2_FXP_STAT, 
	    path, strlen(path));

	return(get_decode_stat(conn->fd_in, id, quiet));
}

Attrib *
do_lstat(struct sftp_conn *conn, char *path, int quiet)
{
	u_int id;

	if (conn->version == 0) {
		if (quiet)
			debug("Server version does not support lstat operation");
		else
			error("Server version does not support lstat operation");
		return(NULL);
	}

	id = conn->msg_id++;
	send_string_request(conn->fd_out, id, SSH2_FXP_LSTAT, path,
	    strlen(path));

	return(get_decode_stat(conn->fd_in, id, quiet));
}

Attrib *
do_fstat(struct sftp_conn *conn, char *handle, u_int handle_len, int quiet)
{
	u_int id;

	id = conn->msg_id++;
	send_string_request(conn->fd_out, id, SSH2_FXP_FSTAT, handle,
	    handle_len);

	return(get_decode_stat(conn->fd_in, id, quiet));
}

int
do_setstat(struct sftp_conn *conn, char *path, Attrib *a)
{
	u_int status, id;

	id = conn->msg_id++;
	send_string_attrs_request(conn->fd_out, id, SSH2_FXP_SETSTAT, path,
	    strlen(path), a);

	status = get_status(conn->fd_in, id);
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
	send_string_attrs_request(conn->fd_out, id, SSH2_FXP_FSETSTAT, handle,
	    handle_len, a);

	status = get_status(conn->fd_in, id);
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
	send_string_request(conn->fd_out, id, SSH2_FXP_REALPATH, path,
	    strlen(path));

	buffer_init(&msg);

	get_msg(conn->fd_in, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%d != %d)", id, expected_id);

	if (type == SSH2_FXP_STATUS) {
		u_int status = buffer_get_int(&msg);

		error("Couldn't canonicalise: %s", fx2txt(status));
		return(NULL);
	} else if (type != SSH2_FXP_NAME)
		fatal("Expected SSH2_FXP_NAME(%d) packet, got %d",
		    SSH2_FXP_NAME, type);

	count = buffer_get_int(&msg);
	if (count != 1)
		fatal("Got multiple names (%d) from SSH_FXP_REALPATH", count);

	filename = buffer_get_string(&msg, NULL);
	longname = buffer_get_string(&msg, NULL);
	a = decode_attrib(&msg);

	debug3("SSH_FXP_REALPATH %s -> %s", path, filename);

	xfree(longname);

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
	buffer_put_char(&msg, SSH2_FXP_RENAME);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, oldpath);
	buffer_put_cstring(&msg, newpath);
	send_msg(conn->fd_out, &msg);
	debug3("Sent message SSH2_FXP_RENAME \"%s\" -> \"%s\"", oldpath,
	    newpath);
	buffer_free(&msg);

	status = get_status(conn->fd_in, id);
	if (status != SSH2_FX_OK)
		error("Couldn't rename file \"%s\" to \"%s\": %s", oldpath,
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

	/* Send rename request */
	id = conn->msg_id++;
	buffer_put_char(&msg, SSH2_FXP_SYMLINK);
	buffer_put_int(&msg, id);
	buffer_put_cstring(&msg, oldpath);
	buffer_put_cstring(&msg, newpath);
	send_msg(conn->fd_out, &msg);
	debug3("Sent message SSH2_FXP_SYMLINK \"%s\" -> \"%s\"", oldpath,
	    newpath);
	buffer_free(&msg);

	status = get_status(conn->fd_in, id);
	if (status != SSH2_FX_OK)
		error("Couldn't rename file \"%s\" to \"%s\": %s", oldpath,
		    newpath, fx2txt(status));

	return(status);
}

char *
do_readlink(struct sftp_conn *conn, char *path)
{
	Buffer msg;
	u_int type, expected_id, count, id;
	char *filename, *longname;
	Attrib *a;

	expected_id = id = conn->msg_id++;
	send_string_request(conn->fd_out, id, SSH2_FXP_READLINK, path,
	    strlen(path));

	buffer_init(&msg);

	get_msg(conn->fd_in, &msg);
	type = buffer_get_char(&msg);
	id = buffer_get_int(&msg);

	if (id != expected_id)
		fatal("ID mismatch (%d != %d)", id, expected_id);

	if (type == SSH2_FXP_STATUS) {
		u_int status = buffer_get_int(&msg);

		error("Couldn't readlink: %s", fx2txt(status));
		return(NULL);
	} else if (type != SSH2_FXP_NAME)
		fatal("Expected SSH2_FXP_NAME(%d) packet, got %d",
		    SSH2_FXP_NAME, type);

	count = buffer_get_int(&msg);
	if (count != 1)
		fatal("Got multiple names (%d) from SSH_FXP_READLINK", count);

	filename = buffer_get_string(&msg, NULL);
	longname = buffer_get_string(&msg, NULL);
	a = decode_attrib(&msg);

	debug3("SSH_FXP_READLINK %s -> %s", path, filename);

	xfree(longname);

	buffer_free(&msg);

	return(filename);
}

static void
send_read_request(int fd_out, u_int id, u_int64_t offset, u_int len,
    char *handle, u_int handle_len)
{
	Buffer msg;
	
	buffer_init(&msg);
	buffer_clear(&msg);
	buffer_put_char(&msg, SSH2_FXP_READ);
	buffer_put_int(&msg, id);
	buffer_put_string(&msg, handle, handle_len);
	buffer_put_int64(&msg, offset);
	buffer_put_int(&msg, len);
	send_msg(fd_out, &msg);
	buffer_free(&msg);
}	

int
do_download(struct sftp_conn *conn, char *remote_path, char *local_path,
    int pflag)
{
	Attrib junk, *a;
	Buffer msg;
	char *handle;
	int local_fd, status, num_req, max_req, write_error;
	int read_error, write_errno;
	u_int64_t offset, size;
	u_int handle_len, mode, type, id, buflen;
	struct request {
		u_int id;
		u_int len;
		u_int64_t offset;
		TAILQ_ENTRY(request) tq; 
	};
	TAILQ_HEAD(reqhead, request) requests;
	struct request *req;

	TAILQ_INIT(&requests);

	a = do_stat(conn, remote_path, 0);
	if (a == NULL)
		return(-1);

	/* XXX: should we preserve set[ug]id? */
	if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		mode = S_IWRITE | (a->perm & 0777);
	else
		mode = 0666;

	if ((a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS) &&
	    (a->perm & S_IFDIR)) {
		error("Cannot download a directory: %s", remote_path);
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
	send_msg(conn->fd_out, &msg);
	debug3("Sent message SSH2_FXP_OPEN I:%d P:%s", id, remote_path);

	handle = get_handle(conn->fd_in, id, &handle_len);
	if (handle == NULL) {
		buffer_free(&msg);
		return(-1);
	}

	local_fd = open(local_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (local_fd == -1) {
		error("Couldn't open local file \"%s\" for writing: %s",
		    local_path, strerror(errno));
		buffer_free(&msg);
		xfree(handle);
		return(-1);
	}

	/* Read from remote and write to local */
	write_error = read_error = write_errno = num_req = offset = 0;
	max_req = 1;
	while (num_req > 0 || max_req > 0) {
		char *data;
		u_int len;

		/* Send some more requests */
		while (num_req < max_req) {
			debug3("Request range %llu -> %llu (%d/%d)", 
			    offset, offset + buflen - 1, num_req, max_req);
			req = xmalloc(sizeof(*req));
			req->id = conn->msg_id++;
			req->len = buflen;
			req->offset = offset;
			offset += buflen;
			num_req++;
			TAILQ_INSERT_TAIL(&requests, req, tq);
			send_read_request(conn->fd_out, req->id, req->offset, 
			    req->len, handle, handle_len);
		}

		buffer_clear(&msg);
		get_msg(conn->fd_in, &msg);
		type = buffer_get_char(&msg);
		id = buffer_get_int(&msg);
		debug3("Received reply T:%d I:%d R:%d", type, id, max_req);

		/* Find the request in our queue */
		for(req = TAILQ_FIRST(&requests);
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
			xfree(req);
			num_req--;
			break;
		case SSH2_FXP_DATA:
			data = buffer_get_string(&msg, &len);
			debug3("Received data %llu -> %llu", req->offset, 
			    req->offset + len - 1);
			if (len > req->len)
				fatal("Received more data than asked for "
				      "%d > %d", len, req->len);
			if ((lseek(local_fd, req->offset, SEEK_SET) == -1 ||
			     atomicio(write, local_fd, data, len) != len) &&
			    !write_error) {
				write_errno = errno;
				write_error = 1;
				max_req = 0;
			}
			xfree(data);

			if (len == req->len) {
				TAILQ_REMOVE(&requests, req, tq);
				xfree(req);
				num_req--;
			} else {
				/* Resend the request for the missing data */
				debug3("Short data block, re-requesting "
				    "%llu -> %llu (%2d)", req->offset + len, 
					req->offset + req->len - 1, num_req);
				req->id = conn->msg_id++;
				req->len -= len;
				req->offset += len;
				send_read_request(conn->fd_out, req->id, 
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
					    offset, num_req);
					max_req = 1;
				}
				else if (max_req < conn->num_requests + 1) {
					++max_req;
				}
			}
			break;
		default:
			fatal("Expected SSH2_FXP_DATA(%d) packet, got %d",
			    SSH2_FXP_DATA, type);
		}
	}

	/* Sanity check */
	if (TAILQ_FIRST(&requests) != NULL)
		fatal("Transfer complete, but requests still in queue");

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

		/* Override umask and utimes if asked */
		if (pflag && fchmod(local_fd, mode) == -1)
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
	xfree(handle);

	return(status);
}

int
do_upload(struct sftp_conn *conn, char *local_path, char *remote_path,
    int pflag)
{
	int local_fd, status;
	u_int handle_len, id, type;
	u_int64_t offset;
	char *handle, *data;
	Buffer msg;
	struct stat sb;
	Attrib a;
	u_int32_t startid;
	u_int32_t ackid;
	struct outstanding_ack {
		u_int id;
		u_int len;
		u_int64_t offset;
		TAILQ_ENTRY(outstanding_ack) tq; 
	};
	TAILQ_HEAD(ackhead, outstanding_ack) acks;
	struct outstanding_ack *ack;

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
	send_msg(conn->fd_out, &msg);
	debug3("Sent message SSH2_FXP_OPEN I:%d P:%s", id, remote_path);

	buffer_clear(&msg);

	handle = get_handle(conn->fd_in, id, &handle_len);
	if (handle == NULL) {
		close(local_fd);
		buffer_free(&msg);
		return(-1);
	}

	startid = ackid = id + 1;
	data = xmalloc(conn->transfer_buflen);

	/* Read from local and write to remote */
	offset = 0;
	for (;;) {
		int len;

		/*
		 * Can't use atomicio here because it returns 0 on EOF, thus losing
		 * the last block of the file
		 */
		do
			len = read(local_fd, data, conn->transfer_buflen);
		while ((len == -1) && (errno == EINTR || errno == EAGAIN));

		if (len == -1)
			fatal("Couldn't read from \"%s\": %s", local_path,
			    strerror(errno));

		if (len != 0) {
			ack = xmalloc(sizeof(*ack));
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
			send_msg(conn->fd_out, &msg);
			debug3("Sent message SSH2_FXP_WRITE I:%d O:%llu S:%u",
			       id, (u_int64_t)offset, len);
		} else if (TAILQ_FIRST(&acks) == NULL)
			break;

		if (ack == NULL)
			fatal("Unexpected ACK %u", id);

		if (id == startid || len == 0 || 
		    id - ackid >= conn->num_requests) {
			buffer_clear(&msg);
			get_msg(conn->fd_in, &msg);
			type = buffer_get_char(&msg);
			id = buffer_get_int(&msg);

			if (type != SSH2_FXP_STATUS)
				fatal("Expected SSH2_FXP_STATUS(%d) packet, "
				    "got %d", SSH2_FXP_STATUS, type);

			status = buffer_get_int(&msg);
			debug3("SSH2_FXP_STATUS %d", status);

			/* Find the request in our queue */
			for(ack = TAILQ_FIRST(&acks);
			    ack != NULL && ack->id != id;
			    ack = TAILQ_NEXT(ack, tq))
				;
			if (ack == NULL)
				fatal("Can't find request for ID %d", id);
			TAILQ_REMOVE(&acks, ack, tq);

			if (status != SSH2_FX_OK) {
				error("Couldn't write to remote file \"%s\": %s",
				      remote_path, fx2txt(status));
				do_close(conn, handle, handle_len);
				close(local_fd);
				goto done;
			}
			debug3("In write loop, ack for %u %d bytes at %llu", 
			   ack->id, ack->len, ack->offset);
			++ackid;
			free(ack);
		}
		offset += len;
	}
	xfree(data);

	if (close(local_fd) == -1) {
		error("Couldn't close local file \"%s\": %s", local_path,
		    strerror(errno));
		do_close(conn, handle, handle_len);
		status = -1;
		goto done;
	}

	/* Override umask and utimes if asked */
	if (pflag)
		do_fsetstat(conn, handle, handle_len, &a);

	status = do_close(conn, handle, handle_len);

done:
	xfree(handle);
	buffer_free(&msg);
	return(status);
}
