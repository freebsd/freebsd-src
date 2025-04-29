/* $OpenBSD: sftp-server.c,v 1.148 2024/04/30 06:23:51 djm Exp $ */
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "atomicio.h"
#include "xmalloc.h"
#include "sshbuf.h"
#include "ssherr.h"
#include "log.h"
#include "misc.h"
#include "match.h"
#include "uidswap.h"

#include "sftp.h"
#include "sftp-common.h"

char *sftp_realpath(const char *, char *); /* sftp-realpath.c */

/* Maximum data read that we are willing to accept */
#define SFTP_MAX_READ_LENGTH (SFTP_MAX_MSG_LENGTH - 1024)

/* Our verbosity */
static LogLevel log_level = SYSLOG_LEVEL_ERROR;

/* Our client */
static struct passwd *pw = NULL;
static char *client_addr = NULL;

/* input and output queue */
struct sshbuf *iqueue;
struct sshbuf *oqueue;

/* Version of client */
static u_int version;

/* SSH2_FXP_INIT received */
static int init_done;

/* Disable writes */
static int readonly;

/* Requests that are allowed/denied */
static char *request_allowlist, *request_denylist;

/* portable attributes, etc. */
typedef struct Stat Stat;

struct Stat {
	char *name;
	char *long_name;
	Attrib attrib;
};

/* Packet handlers */
static void process_open(u_int32_t id);
static void process_close(u_int32_t id);
static void process_read(u_int32_t id);
static void process_write(u_int32_t id);
static void process_stat(u_int32_t id);
static void process_lstat(u_int32_t id);
static void process_fstat(u_int32_t id);
static void process_setstat(u_int32_t id);
static void process_fsetstat(u_int32_t id);
static void process_opendir(u_int32_t id);
static void process_readdir(u_int32_t id);
static void process_remove(u_int32_t id);
static void process_mkdir(u_int32_t id);
static void process_rmdir(u_int32_t id);
static void process_realpath(u_int32_t id);
static void process_rename(u_int32_t id);
static void process_readlink(u_int32_t id);
static void process_symlink(u_int32_t id);
static void process_extended_posix_rename(u_int32_t id);
static void process_extended_statvfs(u_int32_t id);
static void process_extended_fstatvfs(u_int32_t id);
static void process_extended_hardlink(u_int32_t id);
static void process_extended_fsync(u_int32_t id);
static void process_extended_lsetstat(u_int32_t id);
static void process_extended_limits(u_int32_t id);
static void process_extended_expand(u_int32_t id);
static void process_extended_copy_data(u_int32_t id);
static void process_extended_home_directory(u_int32_t id);
static void process_extended_get_users_groups_by_id(u_int32_t id);
static void process_extended(u_int32_t id);

struct sftp_handler {
	const char *name;	/* user-visible name for fine-grained perms */
	const char *ext_name;	/* extended request name */
	u_int type;		/* packet type, for non extended packets */
	void (*handler)(u_int32_t);
	int does_write;		/* if nonzero, banned for readonly mode */
};

static const struct sftp_handler handlers[] = {
	/* NB. SSH2_FXP_OPEN does the readonly check in the handler itself */
	{ "open", NULL, SSH2_FXP_OPEN, process_open, 0 },
	{ "close", NULL, SSH2_FXP_CLOSE, process_close, 0 },
	{ "read", NULL, SSH2_FXP_READ, process_read, 0 },
	{ "write", NULL, SSH2_FXP_WRITE, process_write, 1 },
	{ "lstat", NULL, SSH2_FXP_LSTAT, process_lstat, 0 },
	{ "fstat", NULL, SSH2_FXP_FSTAT, process_fstat, 0 },
	{ "setstat", NULL, SSH2_FXP_SETSTAT, process_setstat, 1 },
	{ "fsetstat", NULL, SSH2_FXP_FSETSTAT, process_fsetstat, 1 },
	{ "opendir", NULL, SSH2_FXP_OPENDIR, process_opendir, 0 },
	{ "readdir", NULL, SSH2_FXP_READDIR, process_readdir, 0 },
	{ "remove", NULL, SSH2_FXP_REMOVE, process_remove, 1 },
	{ "mkdir", NULL, SSH2_FXP_MKDIR, process_mkdir, 1 },
	{ "rmdir", NULL, SSH2_FXP_RMDIR, process_rmdir, 1 },
	{ "realpath", NULL, SSH2_FXP_REALPATH, process_realpath, 0 },
	{ "stat", NULL, SSH2_FXP_STAT, process_stat, 0 },
	{ "rename", NULL, SSH2_FXP_RENAME, process_rename, 1 },
	{ "readlink", NULL, SSH2_FXP_READLINK, process_readlink, 0 },
	{ "symlink", NULL, SSH2_FXP_SYMLINK, process_symlink, 1 },
	{ NULL, NULL, 0, NULL, 0 }
};

/* SSH2_FXP_EXTENDED submessages */
static const struct sftp_handler extended_handlers[] = {
	{ "posix-rename", "posix-rename@openssh.com", 0,
	    process_extended_posix_rename, 1 },
	{ "statvfs", "statvfs@openssh.com", 0, process_extended_statvfs, 0 },
	{ "fstatvfs", "fstatvfs@openssh.com", 0, process_extended_fstatvfs, 0 },
	{ "hardlink", "hardlink@openssh.com", 0, process_extended_hardlink, 1 },
	{ "fsync", "fsync@openssh.com", 0, process_extended_fsync, 1 },
	{ "lsetstat", "lsetstat@openssh.com", 0, process_extended_lsetstat, 1 },
	{ "limits", "limits@openssh.com", 0, process_extended_limits, 0 },
	{ "expand-path", "expand-path@openssh.com", 0,
	    process_extended_expand, 0 },
	{ "copy-data", "copy-data", 0, process_extended_copy_data, 1 },
	{ "home-directory", "home-directory", 0,
	    process_extended_home_directory, 0 },
	{ "users-groups-by-id", "users-groups-by-id@openssh.com", 0,
	    process_extended_get_users_groups_by_id, 0 },
	{ NULL, NULL, 0, NULL, 0 }
};

static const struct sftp_handler *
extended_handler_byname(const char *name)
{
	int i;

	for (i = 0; extended_handlers[i].handler != NULL; i++) {
		if (strcmp(name, extended_handlers[i].ext_name) == 0)
			return &extended_handlers[i];
	}
	return NULL;
}

static int
request_permitted(const struct sftp_handler *h)
{
	char *result;

	if (readonly && h->does_write) {
		verbose("Refusing %s request in read-only mode", h->name);
		return 0;
	}
	if (request_denylist != NULL &&
	    ((result = match_list(h->name, request_denylist, NULL))) != NULL) {
		free(result);
		verbose("Refusing denylisted %s request", h->name);
		return 0;
	}
	if (request_allowlist != NULL &&
	    ((result = match_list(h->name, request_allowlist, NULL))) != NULL) {
		free(result);
		debug2("Permitting allowlisted %s request", h->name);
		return 1;
	}
	if (request_allowlist != NULL) {
		verbose("Refusing non-allowlisted %s request", h->name);
		return 0;
	}
	return 1;
}

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
	case ENOSYS:
		ret = SSH2_FX_OP_UNSUPPORTED;
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
	if (pflags & SSH2_FXF_APPEND)
		flags |= O_APPEND;
	if (pflags & SSH2_FXF_CREAT)
		flags |= O_CREAT;
	if (pflags & SSH2_FXF_TRUNC)
		flags |= O_TRUNC;
	if (pflags & SSH2_FXF_EXCL)
		flags |= O_EXCL;
	return flags;
}

static const char *
string_from_portable(int pflags)
{
	static char ret[128];

	*ret = '\0';

#define PAPPEND(str)	{				\
		if (*ret != '\0')			\
			strlcat(ret, ",", sizeof(ret));	\
		strlcat(ret, str, sizeof(ret));		\
	}

	if (pflags & SSH2_FXF_READ)
		PAPPEND("READ")
	if (pflags & SSH2_FXF_WRITE)
		PAPPEND("WRITE")
	if (pflags & SSH2_FXF_APPEND)
		PAPPEND("APPEND")
	if (pflags & SSH2_FXF_CREAT)
		PAPPEND("CREATE")
	if (pflags & SSH2_FXF_TRUNC)
		PAPPEND("TRUNCATE")
	if (pflags & SSH2_FXF_EXCL)
		PAPPEND("EXCL")

	return ret;
}

/* handle handles */

typedef struct Handle Handle;
struct Handle {
	int use;
	DIR *dirp;
	int fd;
	int flags;
	char *name;
	u_int64_t bytes_read, bytes_write;
	int next_unused;
};

enum {
	HANDLE_UNUSED,
	HANDLE_DIR,
	HANDLE_FILE
};

static Handle *handles = NULL;
static u_int num_handles = 0;
static int first_unused_handle = -1;

static void handle_unused(int i)
{
	handles[i].use = HANDLE_UNUSED;
	handles[i].next_unused = first_unused_handle;
	first_unused_handle = i;
}

static int
handle_new(int use, const char *name, int fd, int flags, DIR *dirp)
{
	int i;

	if (first_unused_handle == -1) {
		if (num_handles + 1 <= num_handles)
			return -1;
		num_handles++;
		handles = xreallocarray(handles, num_handles, sizeof(Handle));
		handle_unused(num_handles - 1);
	}

	i = first_unused_handle;
	first_unused_handle = handles[i].next_unused;

	handles[i].use = use;
	handles[i].dirp = dirp;
	handles[i].fd = fd;
	handles[i].flags = flags;
	handles[i].name = xstrdup(name);
	handles[i].bytes_read = handles[i].bytes_write = 0;

	return i;
}

static int
handle_is_ok(int i, int type)
{
	return i >= 0 && (u_int)i < num_handles && handles[i].use == type;
}

static int
handle_to_string(int handle, u_char **stringp, int *hlenp)
{
	if (stringp == NULL || hlenp == NULL)
		return -1;
	*stringp = xmalloc(sizeof(int32_t));
	put_u32(*stringp, handle);
	*hlenp = sizeof(int32_t);
	return 0;
}

static int
handle_from_string(const u_char *handle, u_int hlen)
{
	int val;

	if (hlen != sizeof(int32_t))
		return -1;
	val = get_u32(handle);
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
handle_to_flags(int handle)
{
	if (handle_is_ok(handle, HANDLE_FILE))
		return handles[handle].flags;
	return 0;
}

static void
handle_update_read(int handle, ssize_t bytes)
{
	if (handle_is_ok(handle, HANDLE_FILE) && bytes > 0)
		handles[handle].bytes_read += bytes;
}

static void
handle_update_write(int handle, ssize_t bytes)
{
	if (handle_is_ok(handle, HANDLE_FILE) && bytes > 0)
		handles[handle].bytes_write += bytes;
}

static u_int64_t
handle_bytes_read(int handle)
{
	if (handle_is_ok(handle, HANDLE_FILE))
		return (handles[handle].bytes_read);
	return 0;
}

static u_int64_t
handle_bytes_write(int handle)
{
	if (handle_is_ok(handle, HANDLE_FILE))
		return (handles[handle].bytes_write);
	return 0;
}

static int
handle_close(int handle)
{
	int ret = -1;

	if (handle_is_ok(handle, HANDLE_FILE)) {
		ret = close(handles[handle].fd);
		free(handles[handle].name);
		handle_unused(handle);
	} else if (handle_is_ok(handle, HANDLE_DIR)) {
		ret = closedir(handles[handle].dirp);
		free(handles[handle].name);
		handle_unused(handle);
	} else {
		errno = ENOENT;
	}
	return ret;
}

static void
handle_log_close(int handle, char *emsg)
{
	if (handle_is_ok(handle, HANDLE_FILE)) {
		logit("%s%sclose \"%s\" bytes read %llu written %llu",
		    emsg == NULL ? "" : emsg, emsg == NULL ? "" : " ",
		    handle_to_name(handle),
		    (unsigned long long)handle_bytes_read(handle),
		    (unsigned long long)handle_bytes_write(handle));
	} else {
		logit("%s%sclosedir \"%s\"",
		    emsg == NULL ? "" : emsg, emsg == NULL ? "" : " ",
		    handle_to_name(handle));
	}
}

static void
handle_log_exit(void)
{
	u_int i;

	for (i = 0; i < num_handles; i++)
		if (handles[i].use != HANDLE_UNUSED)
			handle_log_close(i, "forced");
}

static int
get_handle(struct sshbuf *queue, int *hp)
{
	u_char *handle;
	int r;
	size_t hlen;

	*hp = -1;
	if ((r = sshbuf_get_string(queue, &handle, &hlen)) != 0)
		return r;
	if (hlen < 256)
		*hp = handle_from_string(handle, hlen);
	free(handle);
	return 0;
}

/* send replies */

static void
send_msg(struct sshbuf *m)
{
	int r;

	if ((r = sshbuf_put_stringb(oqueue, m)) != 0)
		fatal_fr(r, "enqueue");
	sshbuf_reset(m);
}

static const char *
status_to_message(u_int32_t status)
{
	static const char * const status_messages[] = {
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
	return (status_messages[MINIMUM(status,SSH2_FX_MAX)]);
}

static void
send_status_errmsg(u_int32_t id, u_int32_t status, const char *errmsg)
{
	struct sshbuf *msg;
	int r;

	debug3("request %u: sent status %u", id, status);
	if (log_level > SYSLOG_LEVEL_VERBOSE ||
	    (status != SSH2_FX_OK && status != SSH2_FX_EOF))
		logit("sent status %s", status_to_message(status));
	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH2_FXP_STATUS)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    (r = sshbuf_put_u32(msg, status)) != 0)
		fatal_fr(r, "compose");
	if (version >= 3) {
		if ((r = sshbuf_put_cstring(msg, errmsg == NULL ?
		    status_to_message(status) : errmsg)) != 0 ||
		    (r = sshbuf_put_cstring(msg, "")) != 0)
			fatal_fr(r, "compose message");
	}
	send_msg(msg);
	sshbuf_free(msg);
}

static void
send_status(u_int32_t id, u_int32_t status)
{
	send_status_errmsg(id, status, NULL);
}

static void
send_data_or_handle(char type, u_int32_t id, const u_char *data, int dlen)
{
	struct sshbuf *msg;
	int r;

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, type)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    (r = sshbuf_put_string(msg, data, dlen)) != 0)
		fatal_fr(r, "compose");
	send_msg(msg);
	sshbuf_free(msg);
}

static void
send_data(u_int32_t id, const u_char *data, int dlen)
{
	debug("request %u: sent data len %d", id, dlen);
	send_data_or_handle(SSH2_FXP_DATA, id, data, dlen);
}

static void
send_handle(u_int32_t id, int handle)
{
	u_char *string;
	int hlen;

	handle_to_string(handle, &string, &hlen);
	debug("request %u: sent handle %d", id, handle);
	send_data_or_handle(SSH2_FXP_HANDLE, id, string, hlen);
	free(string);
}

static void
send_names(u_int32_t id, int count, const Stat *stats)
{
	struct sshbuf *msg;
	int i, r;

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH2_FXP_NAME)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    (r = sshbuf_put_u32(msg, count)) != 0)
		fatal_fr(r, "compose");
	debug("request %u: sent names count %d", id, count);
	for (i = 0; i < count; i++) {
		if ((r = sshbuf_put_cstring(msg, stats[i].name)) != 0 ||
		    (r = sshbuf_put_cstring(msg, stats[i].long_name)) != 0 ||
		    (r = encode_attrib(msg, &stats[i].attrib)) != 0)
			fatal_fr(r, "compose filenames/attrib");
	}
	send_msg(msg);
	sshbuf_free(msg);
}

static void
send_attrib(u_int32_t id, const Attrib *a)
{
	struct sshbuf *msg;
	int r;

	debug("request %u: sent attrib have 0x%x", id, a->flags);
	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH2_FXP_ATTRS)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    (r = encode_attrib(msg, a)) != 0)
		fatal_fr(r, "compose");
	send_msg(msg);
	sshbuf_free(msg);
}

static void
send_statvfs(u_int32_t id, struct statvfs *st)
{
	struct sshbuf *msg;
	u_int64_t flag;
	int r;

	flag = (st->f_flag & ST_RDONLY) ? SSH2_FXE_STATVFS_ST_RDONLY : 0;
	flag |= (st->f_flag & ST_NOSUID) ? SSH2_FXE_STATVFS_ST_NOSUID : 0;

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH2_FXP_EXTENDED_REPLY)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_bsize)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_frsize)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_blocks)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_bfree)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_bavail)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_files)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_ffree)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_favail)) != 0 ||
	    (r = sshbuf_put_u64(msg, FSID_TO_ULONG(st->f_fsid))) != 0 ||
	    (r = sshbuf_put_u64(msg, flag)) != 0 ||
	    (r = sshbuf_put_u64(msg, st->f_namemax)) != 0)
		fatal_fr(r, "compose");
	send_msg(msg);
	sshbuf_free(msg);
}

/*
 * Prepare SSH2_FXP_VERSION extension advertisement for a single extension.
 * The extension is checked for permission prior to advertisement.
 */
static int
compose_extension(struct sshbuf *msg, const char *name, const char *ver)
{
	int r;
	const struct sftp_handler *exthnd;

	if ((exthnd = extended_handler_byname(name)) == NULL)
		fatal_f("internal error: no handler for %s", name);
	if (!request_permitted(exthnd)) {
		debug2_f("refusing to advertise disallowed extension %s", name);
		return 0;
	}
	if ((r = sshbuf_put_cstring(msg, name)) != 0 ||
	    (r = sshbuf_put_cstring(msg, ver)) != 0)
		fatal_fr(r, "compose %s", name);
	return 0;
}

/* parse incoming */

static void
process_init(void)
{
	struct sshbuf *msg;
	int r;

	if ((r = sshbuf_get_u32(iqueue, &version)) != 0)
		fatal_fr(r, "parse");
	verbose("received client version %u", version);
	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH2_FXP_VERSION)) != 0 ||
	    (r = sshbuf_put_u32(msg, SSH2_FILEXFER_VERSION)) != 0)
		fatal_fr(r, "compose");

	 /* extension advertisements */
	compose_extension(msg, "posix-rename@openssh.com", "1");
	compose_extension(msg, "statvfs@openssh.com", "2");
	compose_extension(msg, "fstatvfs@openssh.com", "2");
	compose_extension(msg, "hardlink@openssh.com", "1");
	compose_extension(msg, "fsync@openssh.com", "1");
	compose_extension(msg, "lsetstat@openssh.com", "1");
	compose_extension(msg, "limits@openssh.com", "1");
	compose_extension(msg, "expand-path@openssh.com", "1");
	compose_extension(msg, "copy-data", "1");
	compose_extension(msg, "home-directory", "1");
	compose_extension(msg, "users-groups-by-id@openssh.com", "1");

	send_msg(msg);
	sshbuf_free(msg);
}

static void
process_open(u_int32_t id)
{
	u_int32_t pflags;
	Attrib a;
	char *name;
	int r, handle, fd, flags, mode, status = SSH2_FX_FAILURE;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = sshbuf_get_u32(iqueue, &pflags)) != 0 || /* portable flags */
	    (r = decode_attrib(iqueue, &a)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: open flags %d", id, pflags);
	flags = flags_from_portable(pflags);
	mode = (a.flags & SSH2_FILEXFER_ATTR_PERMISSIONS) ? a.perm : 0666;
	logit("open \"%s\" flags %s mode 0%o",
	    name, string_from_portable(pflags), mode);
	if (readonly &&
	    ((flags & O_ACCMODE) != O_RDONLY ||
	    (flags & (O_CREAT|O_TRUNC)) != 0)) {
		verbose("Refusing open request in read-only mode");
		status = SSH2_FX_PERMISSION_DENIED;
	} else {
		fd = open(name, flags, mode);
		if (fd == -1) {
			status = errno_to_portable(errno);
		} else {
			handle = handle_new(HANDLE_FILE, name, fd, flags, NULL);
			if (handle < 0) {
				close(fd);
			} else {
				send_handle(id, handle);
				status = SSH2_FX_OK;
			}
		}
	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
	free(name);
}

static void
process_close(u_int32_t id)
{
	int r, handle, ret, status = SSH2_FX_FAILURE;

	if ((r = get_handle(iqueue, &handle)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: close handle %u", id, handle);
	handle_log_close(handle, NULL);
	ret = handle_close(handle);
	status = (ret == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
}

static void
process_read(u_int32_t id)
{
	static u_char *buf;
	static size_t buflen;
	u_int32_t len;
	int r, handle, fd, ret, status = SSH2_FX_FAILURE;
	u_int64_t off;

	if ((r = get_handle(iqueue, &handle)) != 0 ||
	    (r = sshbuf_get_u64(iqueue, &off)) != 0 ||
	    (r = sshbuf_get_u32(iqueue, &len)) != 0)
		fatal_fr(r, "parse");

	debug("request %u: read \"%s\" (handle %d) off %llu len %u",
	    id, handle_to_name(handle), handle, (unsigned long long)off, len);
	if ((fd = handle_to_fd(handle)) == -1)
		goto out;
	if (len > SFTP_MAX_READ_LENGTH) {
		debug2("read change len %u to %u", len, SFTP_MAX_READ_LENGTH);
		len = SFTP_MAX_READ_LENGTH;
	}
	if (len > buflen) {
		debug3_f("allocate %zu => %u", buflen, len);
		if ((buf = realloc(buf, len)) == NULL)
			fatal_f("realloc failed");
		buflen = len;
	}
	if (lseek(fd, off, SEEK_SET) == -1) {
		status = errno_to_portable(errno);
		error_f("seek \"%.100s\": %s", handle_to_name(handle),
		    strerror(errno));
		goto out;
	}
	if (len == 0) {
		/* weird, but not strictly disallowed */
		ret = 0;
	} else if ((ret = read(fd, buf, len)) == -1) {
		status = errno_to_portable(errno);
		error_f("read \"%.100s\": %s", handle_to_name(handle),
		    strerror(errno));
		goto out;
	} else if (ret == 0) {
		status = SSH2_FX_EOF;
		goto out;
	}
	send_data(id, buf, ret);
	handle_update_read(handle, ret);
	/* success */
	status = SSH2_FX_OK;
 out:
	if (status != SSH2_FX_OK)
		send_status(id, status);
}

static void
process_write(u_int32_t id)
{
	u_int64_t off;
	size_t len;
	int r, handle, fd, ret, status;
	u_char *data;

	if ((r = get_handle(iqueue, &handle)) != 0 ||
	    (r = sshbuf_get_u64(iqueue, &off)) != 0 ||
	    (r = sshbuf_get_string(iqueue, &data, &len)) != 0)
		fatal_fr(r, "parse");

	debug("request %u: write \"%s\" (handle %d) off %llu len %zu",
	    id, handle_to_name(handle), handle, (unsigned long long)off, len);
	fd = handle_to_fd(handle);

	if (fd < 0)
		status = SSH2_FX_FAILURE;
	else {
		if (!(handle_to_flags(handle) & O_APPEND) &&
		    lseek(fd, off, SEEK_SET) == -1) {
			status = errno_to_portable(errno);
			error_f("seek \"%.100s\": %s", handle_to_name(handle),
			    strerror(errno));
		} else {
/* XXX ATOMICIO ? */
			ret = write(fd, data, len);
			if (ret == -1) {
				status = errno_to_portable(errno);
				error_f("write \"%.100s\": %s",
				    handle_to_name(handle), strerror(errno));
			} else if ((size_t)ret == len) {
				status = SSH2_FX_OK;
				handle_update_write(handle, ret);
			} else {
				debug2_f("nothing at all written");
				status = SSH2_FX_FAILURE;
			}
		}
	}
	send_status(id, status);
	free(data);
}

static void
process_do_stat(u_int32_t id, int do_lstat)
{
	Attrib a;
	struct stat st;
	char *name;
	int r, status = SSH2_FX_FAILURE;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: %sstat", id, do_lstat ? "l" : "");
	verbose("%sstat name \"%s\"", do_lstat ? "l" : "", name);
	r = do_lstat ? lstat(name, &st) : stat(name, &st);
	if (r == -1) {
		status = errno_to_portable(errno);
	} else {
		stat_to_attrib(&st, &a);
		send_attrib(id, &a);
		status = SSH2_FX_OK;
	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
	free(name);
}

static void
process_stat(u_int32_t id)
{
	process_do_stat(id, 0);
}

static void
process_lstat(u_int32_t id)
{
	process_do_stat(id, 1);
}

static void
process_fstat(u_int32_t id)
{
	Attrib a;
	struct stat st;
	int fd, r, handle, status = SSH2_FX_FAILURE;

	if ((r = get_handle(iqueue, &handle)) != 0)
		fatal_fr(r, "parse");
	debug("request %u: fstat \"%s\" (handle %u)",
	    id, handle_to_name(handle), handle);
	fd = handle_to_fd(handle);
	if (fd >= 0) {
		r = fstat(fd, &st);
		if (r == -1) {
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

static struct timespec *
attrib_to_ts(const Attrib *a)
{
	static struct timespec ts[2];

	ts[0].tv_sec = a->atime;
	ts[0].tv_nsec = 0;
	ts[1].tv_sec = a->mtime;
	ts[1].tv_nsec = 0;
	return ts;
}

static void
process_setstat(u_int32_t id)
{
	Attrib a;
	char *name;
	int r, status = SSH2_FX_OK;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = decode_attrib(iqueue, &a)) != 0)
		fatal_fr(r, "parse");

	debug("request %u: setstat name \"%s\"", id, name);
	if (a.flags & SSH2_FILEXFER_ATTR_SIZE) {
		logit("set \"%s\" size %llu",
		    name, (unsigned long long)a.size);
		r = truncate(name, a.size);
		if (r == -1)
			status = errno_to_portable(errno);
	}
	if (a.flags & SSH2_FILEXFER_ATTR_PERMISSIONS) {
		logit("set \"%s\" mode %04o", name, a.perm);
		r = chmod(name, a.perm & 07777);
		if (r == -1)
			status = errno_to_portable(errno);
	}
	if (a.flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
		char buf[64];
		time_t t = a.mtime;

		strftime(buf, sizeof(buf), "%Y%m%d-%H:%M:%S",
		    localtime(&t));
		logit("set \"%s\" modtime %s", name, buf);
		r = utimes(name, attrib_to_tv(&a));
		if (r == -1)
			status = errno_to_portable(errno);
	}
	if (a.flags & SSH2_FILEXFER_ATTR_UIDGID) {
		logit("set \"%s\" owner %lu group %lu", name,
		    (u_long)a.uid, (u_long)a.gid);
		r = chown(name, a.uid, a.gid);
		if (r == -1)
			status = errno_to_portable(errno);
	}
	send_status(id, status);
	free(name);
}

static void
process_fsetstat(u_int32_t id)
{
	Attrib a;
	int handle, fd, r;
	int status = SSH2_FX_OK;

	if ((r = get_handle(iqueue, &handle)) != 0 ||
	    (r = decode_attrib(iqueue, &a)) != 0)
		fatal_fr(r, "parse");

	debug("request %u: fsetstat handle %d", id, handle);
	fd = handle_to_fd(handle);
	if (fd < 0)
		status = SSH2_FX_FAILURE;
	else {
		char *name = handle_to_name(handle);

		if (a.flags & SSH2_FILEXFER_ATTR_SIZE) {
			logit("set \"%s\" size %llu",
			    name, (unsigned long long)a.size);
			r = ftruncate(fd, a.size);
			if (r == -1)
				status = errno_to_portable(errno);
		}
		if (a.flags & SSH2_FILEXFER_ATTR_PERMISSIONS) {
			logit("set \"%s\" mode %04o", name, a.perm);
#ifdef HAVE_FCHMOD
			r = fchmod(fd, a.perm & 07777);
#else
			r = chmod(name, a.perm & 07777);
#endif
			if (r == -1)
				status = errno_to_portable(errno);
		}
		if (a.flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
			char buf[64];
			time_t t = a.mtime;

			strftime(buf, sizeof(buf), "%Y%m%d-%H:%M:%S",
			    localtime(&t));
			logit("set \"%s\" modtime %s", name, buf);
#ifdef HAVE_FUTIMES
			r = futimes(fd, attrib_to_tv(&a));
#else
			r = utimes(name, attrib_to_tv(&a));
#endif
			if (r == -1)
				status = errno_to_portable(errno);
		}
		if (a.flags & SSH2_FILEXFER_ATTR_UIDGID) {
			logit("set \"%s\" owner %lu group %lu", name,
			    (u_long)a.uid, (u_long)a.gid);
#ifdef HAVE_FCHOWN
			r = fchown(fd, a.uid, a.gid);
#else
			r = chown(name, a.uid, a.gid);
#endif
			if (r == -1)
				status = errno_to_portable(errno);
		}
	}
	send_status(id, status);
}

static void
process_opendir(u_int32_t id)
{
	DIR *dirp = NULL;
	char *path;
	int r, handle, status = SSH2_FX_FAILURE;

	if ((r = sshbuf_get_cstring(iqueue, &path, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: opendir", id);
	logit("opendir \"%s\"", path);
	dirp = opendir(path);
	if (dirp == NULL) {
		status = errno_to_portable(errno);
	} else {
		handle = handle_new(HANDLE_DIR, path, 0, 0, dirp);
		if (handle < 0) {
			closedir(dirp);
		} else {
			send_handle(id, handle);
			status = SSH2_FX_OK;
		}

	}
	if (status != SSH2_FX_OK)
		send_status(id, status);
	free(path);
}

static void
process_readdir(u_int32_t id)
{
	DIR *dirp;
	struct dirent *dp;
	char *path;
	int r, handle;

	if ((r = get_handle(iqueue, &handle)) != 0)
		fatal_fr(r, "parse");

	debug("request %u: readdir \"%s\" (handle %d)", id,
	    handle_to_name(handle), handle);
	dirp = handle_to_dir(handle);
	path = handle_to_name(handle);
	if (dirp == NULL || path == NULL) {
		send_status(id, SSH2_FX_FAILURE);
	} else {
		struct stat st;
		char pathname[PATH_MAX];
		Stat *stats;
		int nstats = 10, count = 0, i;

		stats = xcalloc(nstats, sizeof(Stat));
		while ((dp = readdir(dirp)) != NULL) {
			if (count >= nstats) {
				nstats *= 2;
				stats = xreallocarray(stats, nstats, sizeof(Stat));
			}
/* XXX OVERFLOW ? */
			snprintf(pathname, sizeof pathname, "%s%s%s", path,
			    strcmp(path, "/") ? "/" : "", dp->d_name);
			if (lstat(pathname, &st) == -1)
				continue;
			stat_to_attrib(&st, &(stats[count].attrib));
			stats[count].name = xstrdup(dp->d_name);
			stats[count].long_name = ls_file(dp->d_name, &st,
			    0, 0, NULL, NULL);
			count++;
			/* send up to 100 entries in one message */
			/* XXX check packet size instead */
			if (count == 100)
				break;
		}
		if (count > 0) {
			send_names(id, count, stats);
			for (i = 0; i < count; i++) {
				free(stats[i].name);
				free(stats[i].long_name);
			}
		} else {
			send_status(id, SSH2_FX_EOF);
		}
		free(stats);
	}
}

static void
process_remove(u_int32_t id)
{
	char *name;
	int r, status = SSH2_FX_FAILURE;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: remove", id);
	logit("remove name \"%s\"", name);
	r = unlink(name);
	status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	free(name);
}

static void
process_mkdir(u_int32_t id)
{
	Attrib a;
	char *name;
	int r, mode, status = SSH2_FX_FAILURE;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = decode_attrib(iqueue, &a)) != 0)
		fatal_fr(r, "parse");

	mode = (a.flags & SSH2_FILEXFER_ATTR_PERMISSIONS) ?
	    a.perm & 07777 : 0777;
	debug3("request %u: mkdir", id);
	logit("mkdir name \"%s\" mode 0%o", name, mode);
	r = mkdir(name, mode);
	status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	free(name);
}

static void
process_rmdir(u_int32_t id)
{
	char *name;
	int r, status;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: rmdir", id);
	logit("rmdir name \"%s\"", name);
	r = rmdir(name);
	status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	free(name);
}

static void
process_realpath(u_int32_t id)
{
	char resolvedname[PATH_MAX];
	char *path;
	int r;

	if ((r = sshbuf_get_cstring(iqueue, &path, NULL)) != 0)
		fatal_fr(r, "parse");

	if (path[0] == '\0') {
		free(path);
		path = xstrdup(".");
	}
	debug3("request %u: realpath", id);
	verbose("realpath \"%s\"", path);
	if (sftp_realpath(path, resolvedname) == NULL) {
		send_status(id, errno_to_portable(errno));
	} else {
		Stat s;
		attrib_clear(&s.attrib);
		s.name = s.long_name = resolvedname;
		send_names(id, 1, &s);
	}
	free(path);
}

static void
process_rename(u_int32_t id)
{
	char *oldpath, *newpath;
	int r, status;
	struct stat sb;

	if ((r = sshbuf_get_cstring(iqueue, &oldpath, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &newpath, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: rename", id);
	logit("rename old \"%s\" new \"%s\"", oldpath, newpath);
	status = SSH2_FX_FAILURE;
	if (lstat(oldpath, &sb) == -1)
		status = errno_to_portable(errno);
	else if (S_ISREG(sb.st_mode)) {
		/* Race-free rename of regular files */
		if (link(oldpath, newpath) == -1) {
			if (errno == EOPNOTSUPP || errno == ENOSYS
#ifdef EXDEV
			    || errno == EXDEV
#endif
#ifdef LINK_OPNOTSUPP_ERRNO
			    || errno == LINK_OPNOTSUPP_ERRNO
#endif
			    ) {
				struct stat st;

				/*
				 * fs doesn't support links, so fall back to
				 * stat+rename.  This is racy.
				 */
				if (stat(newpath, &st) == -1) {
					if (rename(oldpath, newpath) == -1)
						status =
						    errno_to_portable(errno);
					else
						status = SSH2_FX_OK;
				}
			} else {
				status = errno_to_portable(errno);
			}
		} else if (unlink(oldpath) == -1) {
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
	free(oldpath);
	free(newpath);
}

static void
process_readlink(u_int32_t id)
{
	int r, len;
	char buf[PATH_MAX];
	char *path;

	if ((r = sshbuf_get_cstring(iqueue, &path, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: readlink", id);
	verbose("readlink \"%s\"", path);
	if ((len = readlink(path, buf, sizeof(buf) - 1)) == -1)
		send_status(id, errno_to_portable(errno));
	else {
		Stat s;

		buf[len] = '\0';
		attrib_clear(&s.attrib);
		s.name = s.long_name = buf;
		send_names(id, 1, &s);
	}
	free(path);
}

static void
process_symlink(u_int32_t id)
{
	char *oldpath, *newpath;
	int r, status;

	if ((r = sshbuf_get_cstring(iqueue, &oldpath, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &newpath, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: symlink", id);
	logit("symlink old \"%s\" new \"%s\"", oldpath, newpath);
	/* this will fail if 'newpath' exists */
	r = symlink(oldpath, newpath);
	status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	free(oldpath);
	free(newpath);
}

static void
process_extended_posix_rename(u_int32_t id)
{
	char *oldpath, *newpath;
	int r, status;

	if ((r = sshbuf_get_cstring(iqueue, &oldpath, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &newpath, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: posix-rename", id);
	logit("posix-rename old \"%s\" new \"%s\"", oldpath, newpath);
	r = rename(oldpath, newpath);
	status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	free(oldpath);
	free(newpath);
}

static void
process_extended_statvfs(u_int32_t id)
{
	char *path;
	struct statvfs st;
	int r;

	if ((r = sshbuf_get_cstring(iqueue, &path, NULL)) != 0)
		fatal_fr(r, "parse");
	debug3("request %u: statvfs", id);
	logit("statvfs \"%s\"", path);

	if (statvfs(path, &st) != 0)
		send_status(id, errno_to_portable(errno));
	else
		send_statvfs(id, &st);
	free(path);
}

static void
process_extended_fstatvfs(u_int32_t id)
{
	int r, handle, fd;
	struct statvfs st;

	if ((r = get_handle(iqueue, &handle)) != 0)
		fatal_fr(r, "parse");
	debug("request %u: fstatvfs \"%s\" (handle %u)",
	    id, handle_to_name(handle), handle);
	if ((fd = handle_to_fd(handle)) < 0) {
		send_status(id, SSH2_FX_FAILURE);
		return;
	}
	if (fstatvfs(fd, &st) != 0)
		send_status(id, errno_to_portable(errno));
	else
		send_statvfs(id, &st);
}

static void
process_extended_hardlink(u_int32_t id)
{
	char *oldpath, *newpath;
	int r, status;

	if ((r = sshbuf_get_cstring(iqueue, &oldpath, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &newpath, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: hardlink", id);
	logit("hardlink old \"%s\" new \"%s\"", oldpath, newpath);
	r = link(oldpath, newpath);
	status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	send_status(id, status);
	free(oldpath);
	free(newpath);
}

static void
process_extended_fsync(u_int32_t id)
{
	int handle, fd, r, status = SSH2_FX_OP_UNSUPPORTED;

	if ((r = get_handle(iqueue, &handle)) != 0)
		fatal_fr(r, "parse");
	debug3("request %u: fsync (handle %u)", id, handle);
	verbose("fsync \"%s\"", handle_to_name(handle));
	if ((fd = handle_to_fd(handle)) < 0)
		status = SSH2_FX_NO_SUCH_FILE;
	else if (handle_is_ok(handle, HANDLE_FILE)) {
		r = fsync(fd);
		status = (r == -1) ? errno_to_portable(errno) : SSH2_FX_OK;
	}
	send_status(id, status);
}

static void
process_extended_lsetstat(u_int32_t id)
{
	Attrib a;
	char *name;
	int r, status = SSH2_FX_OK;

	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = decode_attrib(iqueue, &a)) != 0)
		fatal_fr(r, "parse");

	debug("request %u: lsetstat name \"%s\"", id, name);
	if (a.flags & SSH2_FILEXFER_ATTR_SIZE) {
		/* nonsensical for links */
		status = SSH2_FX_BAD_MESSAGE;
		goto out;
	}
	if (a.flags & SSH2_FILEXFER_ATTR_PERMISSIONS) {
		logit("set \"%s\" mode %04o", name, a.perm);
		r = fchmodat(AT_FDCWD, name,
		    a.perm & 07777, AT_SYMLINK_NOFOLLOW);
		if (r == -1)
			status = errno_to_portable(errno);
	}
	if (a.flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
		char buf[64];
		time_t t = a.mtime;

		strftime(buf, sizeof(buf), "%Y%m%d-%H:%M:%S",
		    localtime(&t));
		logit("set \"%s\" modtime %s", name, buf);
		r = utimensat(AT_FDCWD, name,
		    attrib_to_ts(&a), AT_SYMLINK_NOFOLLOW);
		if (r == -1)
			status = errno_to_portable(errno);
	}
	if (a.flags & SSH2_FILEXFER_ATTR_UIDGID) {
		logit("set \"%s\" owner %lu group %lu", name,
		    (u_long)a.uid, (u_long)a.gid);
		r = fchownat(AT_FDCWD, name, a.uid, a.gid,
		    AT_SYMLINK_NOFOLLOW);
		if (r == -1)
			status = errno_to_portable(errno);
	}
 out:
	send_status(id, status);
	free(name);
}

static void
process_extended_limits(u_int32_t id)
{
	struct sshbuf *msg;
	int r;
	uint64_t nfiles = 0;
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit rlim;
#endif

	debug("request %u: limits", id);

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	if (getrlimit(RLIMIT_NOFILE, &rlim) != -1 && rlim.rlim_cur > 5)
		nfiles = rlim.rlim_cur - 5; /* stdio(3) + syslog + spare */
#endif

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH2_FXP_EXTENDED_REPLY)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    /* max-packet-length */
	    (r = sshbuf_put_u64(msg, SFTP_MAX_MSG_LENGTH)) != 0 ||
	    /* max-read-length */
	    (r = sshbuf_put_u64(msg, SFTP_MAX_READ_LENGTH)) != 0 ||
	    /* max-write-length */
	    (r = sshbuf_put_u64(msg, SFTP_MAX_MSG_LENGTH - 1024)) != 0 ||
	    /* max-open-handles */
	    (r = sshbuf_put_u64(msg, nfiles)) != 0)
		fatal_fr(r, "compose");
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process_extended_expand(u_int32_t id)
{
	char cwd[PATH_MAX], resolvedname[PATH_MAX];
	char *path, *npath;
	int r;
	Stat s;

	if ((r = sshbuf_get_cstring(iqueue, &path, NULL)) != 0)
		fatal_fr(r, "parse");
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		send_status(id, errno_to_portable(errno));
		goto out;
	}

	debug3("request %u: expand, original \"%s\"", id, path);
	if (path[0] == '\0') {
		/* empty path */
		free(path);
		path = xstrdup(".");
	} else if (*path == '~') {
		/* ~ expand path */
		/* Special-case for "~" and "~/" to respect homedir flag */
		if (strcmp(path, "~") == 0) {
			free(path);
			path = xstrdup(cwd);
		} else if (strncmp(path, "~/", 2) == 0) {
			npath = xstrdup(path + 2);
			free(path);
			xasprintf(&path, "%s/%s", cwd, npath);
			free(npath);
		} else {
			/* ~user expansions */
			if (tilde_expand(path, pw->pw_uid, &npath) != 0) {
				send_status_errmsg(id,
				    errno_to_portable(ENOENT), "no such user");
				goto out;
			}
			free(path);
			path = npath;
		}
	} else if (*path != '/') {
		/* relative path */
		xasprintf(&npath, "%s/%s", cwd, path);
		free(path);
		path = npath;
	}
	verbose("expand \"%s\"", path);
	if (sftp_realpath(path, resolvedname) == NULL) {
		send_status(id, errno_to_portable(errno));
		goto out;
	}
	attrib_clear(&s.attrib);
	s.name = s.long_name = resolvedname;
	send_names(id, 1, &s);
 out:
	free(path);
}

static void
process_extended_copy_data(u_int32_t id)
{
	u_char buf[64*1024];
	int read_handle, read_fd, write_handle, write_fd;
	u_int64_t len, read_off, read_len, write_off;
	int r, copy_until_eof, status = SSH2_FX_OP_UNSUPPORTED;
	size_t ret;

	if ((r = get_handle(iqueue, &read_handle)) != 0 ||
	    (r = sshbuf_get_u64(iqueue, &read_off)) != 0 ||
	    (r = sshbuf_get_u64(iqueue, &read_len)) != 0 ||
	    (r = get_handle(iqueue, &write_handle)) != 0 ||
	    (r = sshbuf_get_u64(iqueue, &write_off)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	debug("request %u: copy-data from \"%s\" (handle %d) off %llu len %llu "
	    "to \"%s\" (handle %d) off %llu",
	    id, handle_to_name(read_handle), read_handle,
	    (unsigned long long)read_off, (unsigned long long)read_len,
	    handle_to_name(write_handle), write_handle,
	    (unsigned long long)write_off);

	/* For read length of 0, we read until EOF. */
	if (read_len == 0) {
		read_len = (u_int64_t)-1 - read_off;
		copy_until_eof = 1;
	} else
		copy_until_eof = 0;

	read_fd = handle_to_fd(read_handle);
	write_fd = handle_to_fd(write_handle);

	/* Disallow reading & writing to the same handle or same path or dirs */
	if (read_handle == write_handle || read_fd < 0 || write_fd < 0 ||
	    !strcmp(handle_to_name(read_handle), handle_to_name(write_handle))) {
		status = SSH2_FX_FAILURE;
		goto out;
	}

	if (lseek(read_fd, read_off, SEEK_SET) < 0) {
		status = errno_to_portable(errno);
		error("%s: read_seek failed", __func__);
		goto out;
	}

	if ((handle_to_flags(write_handle) & O_APPEND) == 0 &&
	    lseek(write_fd, write_off, SEEK_SET) < 0) {
		status = errno_to_portable(errno);
		error("%s: write_seek failed", __func__);
		goto out;
	}

	/* Process the request in chunks. */
	while (read_len > 0 || copy_until_eof) {
		len = MINIMUM(sizeof(buf), read_len);
		read_len -= len;

		ret = atomicio(read, read_fd, buf, len);
		if (ret == 0 && errno == EPIPE) {
			status = copy_until_eof ? SSH2_FX_OK : SSH2_FX_EOF;
			break;
		} else if (ret == 0) {
			status = errno_to_portable(errno);
			error("%s: read failed: %s", __func__, strerror(errno));
			break;
		}
		len = ret;
		handle_update_read(read_handle, len);

		ret = atomicio(vwrite, write_fd, buf, len);
		if (ret != len) {
			status = errno_to_portable(errno);
			error("%s: write failed: %llu != %llu: %s", __func__,
			    (unsigned long long)ret, (unsigned long long)len,
			    strerror(errno));
			break;
		}
		handle_update_write(write_handle, len);
	}

	if (read_len == 0)
		status = SSH2_FX_OK;

 out:
	send_status(id, status);
}

static void
process_extended_home_directory(u_int32_t id)
{
	char *username;
	struct passwd *user_pw;
	int r;
	Stat s;

	if ((r = sshbuf_get_cstring(iqueue, &username, NULL)) != 0)
		fatal_fr(r, "parse");

	debug3("request %u: home-directory \"%s\"", id, username);
	if (username[0] == '\0') {
		user_pw = pw;
	} else if ((user_pw = getpwnam(username)) == NULL) {
		send_status(id, SSH2_FX_FAILURE);
		goto out;
	}

	verbose("home-directory \"%s\"", user_pw->pw_dir);
	attrib_clear(&s.attrib);
	s.name = s.long_name = user_pw->pw_dir;
	send_names(id, 1, &s);
 out:
	free(username);
}

static void
process_extended_get_users_groups_by_id(u_int32_t id)
{
	struct passwd *user_pw;
	struct group *gr;
	struct sshbuf *uids, *gids, *usernames, *groupnames, *msg;
	int r;
	u_int n, nusers = 0, ngroups = 0;
	const char *name;

	if ((usernames = sshbuf_new()) == NULL ||
	    (groupnames = sshbuf_new()) == NULL ||
	    (msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_froms(iqueue, &uids)) != 0 ||
	    (r = sshbuf_froms(iqueue, &gids)) != 0)
		fatal_fr(r, "parse");
	debug_f("uids len = %zu, gids len = %zu",
	    sshbuf_len(uids), sshbuf_len(gids));
	while (sshbuf_len(uids) != 0) {
		if ((r = sshbuf_get_u32(uids, &n)) != 0)
			fatal_fr(r, "parse inner uid");
		user_pw = getpwuid((uid_t)n);
		name = user_pw == NULL ? "" : user_pw->pw_name;
		debug3_f("uid %u => \"%s\"", n, name);
		if ((r = sshbuf_put_cstring(usernames, name)) != 0)
			fatal_fr(r, "assemble uid reply");
		nusers++;
	}
	while (sshbuf_len(gids) != 0) {
		if ((r = sshbuf_get_u32(gids, &n)) != 0)
			fatal_fr(r, "parse inner gid");
		gr = getgrgid((gid_t)n);
		name = gr == NULL ? "" : gr->gr_name;
		debug3_f("gid %u => \"%s\"", n, name);
		if ((r = sshbuf_put_cstring(groupnames, name)) != 0)
			fatal_fr(r, "assemble gid reply");
		nusers++;
	}
	verbose("users-groups-by-id: %u users, %u groups", nusers, ngroups);

	if ((r = sshbuf_put_u8(msg, SSH2_FXP_EXTENDED_REPLY)) != 0 ||
	    (r = sshbuf_put_u32(msg, id)) != 0 ||
	    (r = sshbuf_put_stringb(msg, usernames)) != 0 ||
	    (r = sshbuf_put_stringb(msg, groupnames)) != 0)
		fatal_fr(r, "compose");
	send_msg(msg);

	sshbuf_free(uids);
	sshbuf_free(gids);
	sshbuf_free(usernames);
	sshbuf_free(groupnames);
	sshbuf_free(msg);
}

static void
process_extended(u_int32_t id)
{
	char *request;
	int r;
	const struct sftp_handler *exthand;

	if ((r = sshbuf_get_cstring(iqueue, &request, NULL)) != 0)
		fatal_fr(r, "parse");
	if ((exthand = extended_handler_byname(request)) == NULL) {
		error("Unknown extended request \"%.100s\"", request);
		send_status(id, SSH2_FX_OP_UNSUPPORTED);	/* MUST */
	} else {
		if (!request_permitted(exthand))
			send_status(id, SSH2_FX_PERMISSION_DENIED);
		else
			exthand->handler(id);
	}
	free(request);
}

/* stolen from ssh-agent */

static void
process(void)
{
	u_int msg_len;
	u_int buf_len;
	u_int consumed;
	u_char type;
	const u_char *cp;
	int i, r;
	u_int32_t id;

	buf_len = sshbuf_len(iqueue);
	if (buf_len < 5)
		return;		/* Incomplete message. */
	cp = sshbuf_ptr(iqueue);
	msg_len = get_u32(cp);
	if (msg_len > SFTP_MAX_MSG_LENGTH) {
		error("bad message from %s local user %s",
		    client_addr, pw->pw_name);
		sftp_server_cleanup_exit(11);
	}
	if (buf_len < msg_len + 4)
		return;
	if ((r = sshbuf_consume(iqueue, 4)) != 0)
		fatal_fr(r, "consume");
	buf_len -= 4;
	if ((r = sshbuf_get_u8(iqueue, &type)) != 0)
		fatal_fr(r, "parse type");

	switch (type) {
	case SSH2_FXP_INIT:
		process_init();
		init_done = 1;
		break;
	case SSH2_FXP_EXTENDED:
		if (!init_done)
			fatal("Received extended request before init");
		if ((r = sshbuf_get_u32(iqueue, &id)) != 0)
			fatal_fr(r, "parse extended ID");
		process_extended(id);
		break;
	default:
		if (!init_done)
			fatal("Received %u request before init", type);
		if ((r = sshbuf_get_u32(iqueue, &id)) != 0)
			fatal_fr(r, "parse ID");
		for (i = 0; handlers[i].handler != NULL; i++) {
			if (type == handlers[i].type) {
				if (!request_permitted(&handlers[i])) {
					send_status(id,
					    SSH2_FX_PERMISSION_DENIED);
				} else {
					handlers[i].handler(id);
				}
				break;
			}
		}
		if (handlers[i].handler == NULL)
			error("Unknown message %u", type);
	}
	/* discard the remaining bytes from the current packet */
	if (buf_len < sshbuf_len(iqueue)) {
		error("iqueue grew unexpectedly");
		sftp_server_cleanup_exit(255);
	}
	consumed = buf_len - sshbuf_len(iqueue);
	if (msg_len < consumed) {
		error("msg_len %u < consumed %u", msg_len, consumed);
		sftp_server_cleanup_exit(255);
	}
	if (msg_len > consumed &&
	    (r = sshbuf_consume(iqueue, msg_len - consumed)) != 0)
		fatal_fr(r, "consume");
}

/* Cleanup handler that logs active handles upon normal exit */
void
sftp_server_cleanup_exit(int i)
{
	if (pw != NULL && client_addr != NULL) {
		handle_log_exit();
		logit("session closed for local user %s from [%s]",
		    pw->pw_name, client_addr);
	}
	_exit(i);
}

static void
sftp_server_usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-ehR] [-d start_directory] [-f log_facility] "
	    "[-l log_level]\n\t[-P denied_requests] "
	    "[-p allowed_requests] [-u umask]\n"
	    "       %s -Q protocol_feature\n",
	    __progname, __progname);
	exit(1);
}

int
sftp_server_main(int argc, char **argv, struct passwd *user_pw)
{
	int i, r, in, out, ch, skipargs = 0, log_stderr = 0;
	ssize_t len, olen;
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
	char *cp, *homedir = NULL, uidstr[32], buf[4*4096];
	long mask;

	extern char *optarg;
	extern char *__progname;

	__progname = ssh_get_progname(argv[0]);
	log_init(__progname, log_level, log_facility, log_stderr);

	pw = pwcopy(user_pw);

	while (!skipargs && (ch = getopt(argc, argv,
	    "d:f:l:P:p:Q:u:cehR")) != -1) {
		switch (ch) {
		case 'Q':
			if (strcasecmp(optarg, "requests") != 0) {
				fprintf(stderr, "Invalid query type\n");
				exit(1);
			}
			for (i = 0; handlers[i].handler != NULL; i++)
				printf("%s\n", handlers[i].name);
			for (i = 0; extended_handlers[i].handler != NULL; i++)
				printf("%s\n", extended_handlers[i].name);
			exit(0);
			break;
		case 'R':
			readonly = 1;
			break;
		case 'c':
			/*
			 * Ignore all arguments if we are invoked as a
			 * shell using "sftp-server -c command"
			 */
			skipargs = 1;
			break;
		case 'e':
			log_stderr = 1;
			break;
		case 'l':
			log_level = log_level_number(optarg);
			if (log_level == SYSLOG_LEVEL_NOT_SET)
				error("Invalid log level \"%s\"", optarg);
			break;
		case 'f':
			log_facility = log_facility_number(optarg);
			if (log_facility == SYSLOG_FACILITY_NOT_SET)
				error("Invalid log facility \"%s\"", optarg);
			break;
		case 'd':
			cp = tilde_expand_filename(optarg, user_pw->pw_uid);
			snprintf(uidstr, sizeof(uidstr), "%llu",
			    (unsigned long long)pw->pw_uid);
			homedir = percent_expand(cp, "d", user_pw->pw_dir,
			    "u", user_pw->pw_name, "U", uidstr, (char *)NULL);
			free(cp);
			break;
		case 'p':
			if (request_allowlist != NULL)
				fatal("Permitted requests already set");
			request_allowlist = xstrdup(optarg);
			break;
		case 'P':
			if (request_denylist != NULL)
				fatal("Refused requests already set");
			request_denylist = xstrdup(optarg);
			break;
		case 'u':
			errno = 0;
			mask = strtol(optarg, &cp, 8);
			if (mask < 0 || mask > 0777 || *cp != '\0' ||
			    cp == optarg || (mask == 0 && errno != 0))
				fatal("Invalid umask \"%s\"", optarg);
			(void)umask((mode_t)mask);
			break;
		case 'h':
		default:
			sftp_server_usage();
		}
	}

	log_init(__progname, log_level, log_facility, log_stderr);

	/*
	 * On platforms where we can, avoid making /proc/self/{mem,maps}
	 * available to the user so that sftp access doesn't automatically
	 * imply arbitrary code execution access that will break
	 * restricted configurations.
	 */
	platform_disable_tracing(1);	/* strict */

	/* Drop any fine-grained privileges we don't need */
	platform_pledge_sftp_server();

	if ((cp = getenv("SSH_CONNECTION")) != NULL) {
		client_addr = xstrdup(cp);
		if ((cp = strchr(client_addr, ' ')) == NULL) {
			error("Malformed SSH_CONNECTION variable: \"%s\"",
			    getenv("SSH_CONNECTION"));
			sftp_server_cleanup_exit(255);
		}
		*cp = '\0';
	} else
		client_addr = xstrdup("UNKNOWN");

	logit("session opened for local user %s from [%s]",
	    pw->pw_name, client_addr);

	in = STDIN_FILENO;
	out = STDOUT_FILENO;

#ifdef HAVE_CYGWIN
	setmode(in, O_BINARY);
	setmode(out, O_BINARY);
#endif

	if ((iqueue = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((oqueue = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");

	if (homedir != NULL) {
		if (chdir(homedir) != 0) {
			error("chdir to \"%s\" failed: %s", homedir,
			    strerror(errno));
		}
	}

	for (;;) {
		struct pollfd pfd[2];

		memset(pfd, 0, sizeof pfd);
		pfd[0].fd = pfd[1].fd = -1;

		/*
		 * Ensure that we can read a full buffer and handle
		 * the worst-case length packet it can generate,
		 * otherwise apply backpressure by stopping reads.
		 */
		if ((r = sshbuf_check_reserve(iqueue, sizeof(buf))) == 0 &&
		    (r = sshbuf_check_reserve(oqueue,
		    SFTP_MAX_MSG_LENGTH)) == 0) {
			pfd[0].fd = in;
			pfd[0].events = POLLIN;
		}
		else if (r != SSH_ERR_NO_BUFFER_SPACE)
			fatal_fr(r, "reserve");

		olen = sshbuf_len(oqueue);
		if (olen > 0) {
			pfd[1].fd = out;
			pfd[1].events = POLLOUT;
		}

		if (poll(pfd, 2, -1) == -1) {
			if (errno == EINTR)
				continue;
			error("poll: %s", strerror(errno));
			sftp_server_cleanup_exit(2);
		}

		/* copy stdin to iqueue */
		if (pfd[0].revents & (POLLIN|POLLHUP)) {
			len = read(in, buf, sizeof buf);
			if (len == 0) {
				debug("read eof");
				sftp_server_cleanup_exit(0);
			} else if (len == -1) {
				if (errno != EAGAIN && errno != EINTR) {
					error("read: %s", strerror(errno));
					sftp_server_cleanup_exit(1);
				}
			} else if ((r = sshbuf_put(iqueue, buf, len)) != 0)
				fatal_fr(r, "sshbuf_put");
		}
		/* send oqueue to stdout */
		if (pfd[1].revents & (POLLOUT|POLLHUP)) {
			len = write(out, sshbuf_ptr(oqueue), olen);
			if (len == 0 || (len == -1 && errno == EPIPE)) {
				debug("write eof");
				sftp_server_cleanup_exit(0);
			} else if (len == -1) {
				sftp_server_cleanup_exit(1);
				if (errno != EAGAIN && errno != EINTR) {
					error("write: %s", strerror(errno));
					sftp_server_cleanup_exit(1);
				}
			} else if ((r = sshbuf_consume(oqueue, len)) != 0)
				fatal_fr(r, "consume");
		}

		/*
		 * Process requests from client if we can fit the results
		 * into the output buffer, otherwise stop processing input
		 * and let the output queue drain.
		 */
		r = sshbuf_check_reserve(oqueue, SFTP_MAX_MSG_LENGTH);
		if (r == 0)
			process();
		else if (r != SSH_ERR_NO_BUFFER_SPACE)
			fatal_fr(r, "reserve");
	}
}
