/* $OpenBSD: sftp-client.h,v 1.39 2023/09/08 05:56:13 djm Exp $ */

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

/* Client side of SSH2 filexfer protocol */

#ifndef _SFTP_CLIENT_H
#define _SFTP_CLIENT_H

#ifdef USE_SYSTEM_GLOB
# include <glob.h>
#else
# include "openbsd-compat/glob.h"
#endif

typedef struct SFTP_DIRENT SFTP_DIRENT;

struct SFTP_DIRENT {
	char *filename;
	char *longname;
	Attrib a;
};

/*
 * Used for statvfs responses on the wire from the server, because the
 * server's native format may be larger than the client's.
 */
struct sftp_statvfs {
	u_int64_t f_bsize;
	u_int64_t f_frsize;
	u_int64_t f_blocks;
	u_int64_t f_bfree;
	u_int64_t f_bavail;
	u_int64_t f_files;
	u_int64_t f_ffree;
	u_int64_t f_favail;
	u_int64_t f_fsid;
	u_int64_t f_flag;
	u_int64_t f_namemax;
};

/* Used for limits response on the wire from the server */
struct sftp_limits {
	u_int64_t packet_length;
	u_int64_t read_length;
	u_int64_t write_length;
	u_int64_t open_handles;
};

/* print flag values */
#define SFTP_QUIET		0	/* be quiet during transfers */
#define SFTP_PRINT		1	/* list files and show progress bar */
#define SFTP_PROGRESS_ONLY	2	/* progress bar only */

/*
 * Initialise a SSH filexfer connection. Returns NULL on error or
 * a pointer to a initialized sftp_conn struct on success.
 */
struct sftp_conn *sftp_init(int, int, u_int, u_int, u_int64_t);

u_int sftp_proto_version(struct sftp_conn *);

/* Query server limits */
int sftp_get_limits(struct sftp_conn *, struct sftp_limits *);

/* Close file referred to by 'handle' */
int sftp_close(struct sftp_conn *, const u_char *, u_int);

/* Read contents of 'path' to NULL-terminated array 'dir' */
int sftp_readdir(struct sftp_conn *, const char *, SFTP_DIRENT ***);

/* Frees a NULL-terminated array of SFTP_DIRENTs (eg. from sftp_readdir) */
void sftp_free_dirents(SFTP_DIRENT **);

/* Delete file 'path' */
int sftp_rm(struct sftp_conn *, const char *);

/* Create directory 'path' */
int sftp_mkdir(struct sftp_conn *, const char *, Attrib *, int);

/* Remove directory 'path' */
int sftp_rmdir(struct sftp_conn *, const char *);

/* Get file attributes of 'path' (follows symlinks) */
int sftp_stat(struct sftp_conn *, const char *, int, Attrib *);

/* Get file attributes of 'path' (does not follow symlinks) */
int sftp_lstat(struct sftp_conn *, const char *, int, Attrib *);

/* Set file attributes of 'path' */
int sftp_setstat(struct sftp_conn *, const char *, Attrib *);

/* Set file attributes of open file 'handle' */
int sftp_fsetstat(struct sftp_conn *, const u_char *, u_int, Attrib *);

/* Set file attributes of 'path', not following symlinks */
int sftp_lsetstat(struct sftp_conn *conn, const char *path, Attrib *a);

/* Canonicalise 'path' - caller must free result */
char *sftp_realpath(struct sftp_conn *, const char *);

/* Canonicalisation with tilde expansion (requires server extension) */
char *sftp_expand_path(struct sftp_conn *, const char *);

/* Returns non-zero if server can tilde-expand paths */
int sftp_can_expand_path(struct sftp_conn *);

/* Get statistics for filesystem hosting file at "path" */
int sftp_statvfs(struct sftp_conn *, const char *, struct sftp_statvfs *, int);

/* Rename 'oldpath' to 'newpath' */
int sftp_rename(struct sftp_conn *, const char *, const char *, int);

/* Copy 'oldpath' to 'newpath' */
int sftp_copy(struct sftp_conn *, const char *, const char *);

/* Link 'oldpath' to 'newpath' */
int sftp_hardlink(struct sftp_conn *, const char *, const char *);

/* Rename 'oldpath' to 'newpath' */
int sftp_symlink(struct sftp_conn *, const char *, const char *);

/* Call fsync() on open file 'handle' */
int sftp_fsync(struct sftp_conn *conn, u_char *, u_int);

/*
 * Download 'remote_path' to 'local_path'. Preserve permissions and times
 * if 'pflag' is set
 */
int sftp_download(struct sftp_conn *, const char *, const char *, Attrib *,
    int, int, int, int);

/*
 * Recursively download 'remote_directory' to 'local_directory'. Preserve
 * times if 'pflag' is set
 */
int sftp_download_dir(struct sftp_conn *, const char *, const char *, Attrib *,
    int, int, int, int, int, int);

/*
 * Upload 'local_path' to 'remote_path'. Preserve permissions and times
 * if 'pflag' is set
 */
int sftp_upload(struct sftp_conn *, const char *, const char *,
    int, int, int, int);

/*
 * Recursively upload 'local_directory' to 'remote_directory'. Preserve
 * times if 'pflag' is set
 */
int sftp_upload_dir(struct sftp_conn *, const char *, const char *,
    int, int, int, int, int, int);

/*
 * Download a 'from_path' from the 'from' connection and upload it to
 * to 'to' connection at 'to_path'.
 */
int sftp_crossload(struct sftp_conn *from, struct sftp_conn *to,
    const char *from_path, const char *to_path,
    Attrib *a, int preserve_flag);

/*
 * Recursively download a directory from 'from_path' from the 'from'
 * connection and upload it to 'to' connection at 'to_path'.
 */
int sftp_crossload_dir(struct sftp_conn *from, struct sftp_conn *to,
    const char *from_path, const char *to_path,
    Attrib *dirattrib, int preserve_flag, int print_flag,
    int follow_link_flag);

/*
 * User/group ID to name translation.
 */
int sftp_can_get_users_groups_by_id(struct sftp_conn *conn);
int sftp_get_users_groups_by_id(struct sftp_conn *conn,
    const u_int *uids, u_int nuids,
    const u_int *gids, u_int ngids,
    char ***usernamesp, char ***groupnamesp);

/* Concatenate paths, taking care of slashes. Caller must free result. */
char *sftp_path_append(const char *, const char *);

/* Make absolute path if relative path and CWD is given. Does not modify
 * original if the path is already absolute. */
char *sftp_make_absolute(char *, const char *);

/* Check if remote path is directory */
int sftp_remote_is_dir(struct sftp_conn *conn, const char *path);

/* Check whether path returned from glob(..., GLOB_MARK, ...) is a directory */
int sftp_globpath_is_dir(const char *pathname);

#endif
