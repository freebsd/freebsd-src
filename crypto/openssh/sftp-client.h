/* $OpenBSD: sftp-client.h,v 1.5 2001/04/05 10:42:52 markus Exp $ */

/*
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
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

/* Client side of SSH2 filexfer protocol */

typedef struct SFTP_DIRENT SFTP_DIRENT;

struct SFTP_DIRENT {
	char *filename;
	char *longname;
	Attrib a;
};

/*
 * Initialiase a SSH filexfer connection. Returns -1 on error or
 * protocol version on success.
 */
int do_init(int fd_in, int fd_out);

/* Close file referred to by 'handle' */
int do_close(int fd_in, int fd_out, char *handle, u_int handle_len);

/* List contents of directory 'path' to stdout */
int do_ls(int fd_in, int fd_out, char *path);

/* Read contents of 'path' to NULL-terminated array 'dir' */
int do_readdir(int fd_in, int fd_out, char *path, SFTP_DIRENT ***dir);

/* Frees a NULL-terminated array of SFTP_DIRENTs (eg. from do_readdir) */
void free_sftp_dirents(SFTP_DIRENT **s);

/* Delete file 'path' */
int do_rm(int fd_in, int fd_out, char *path);

/* Create directory 'path' */
int do_mkdir(int fd_in, int fd_out, char *path, Attrib *a);

/* Remove directory 'path' */
int do_rmdir(int fd_in, int fd_out, char *path);

/* Get file attributes of 'path' (follows symlinks) */
Attrib *do_stat(int fd_in, int fd_out, char *path, int quiet);

/* Get file attributes of 'path' (does not follow symlinks) */
Attrib *do_lstat(int fd_in, int fd_out, char *path, int quiet);

/* Get file attributes of open file 'handle' */
Attrib *do_fstat(int fd_in, int fd_out, char *handle, u_int handle_len,
    int quiet);

/* Set file attributes of 'path' */
int do_setstat(int fd_in, int fd_out, char *path, Attrib *a);

/* Set file attributes of open file 'handle' */
int do_fsetstat(int fd_in, int fd_out, char *handle,
    u_int handle_len, Attrib *a);

/* Canonicalise 'path' - caller must free result */
char *do_realpath(int fd_in, int fd_out, char *path);

/* Rename 'oldpath' to 'newpath' */
int do_rename(int fd_in, int fd_out, char *oldpath, char *newpath);

/* Rename 'oldpath' to 'newpath' */
int do_symlink(int fd_in, int fd_out, char *oldpath, char *newpath);

/* Return target of symlink 'path' - caller must free result */
char *do_readlink(int fd_in, int fd_out, char *path);

/* XXX: add callbacks to do_download/do_upload so we can do progress meter */

/*
 * Download 'remote_path' to 'local_path'. Preserve permissions and times
 * if 'pflag' is set
 */
int do_download(int fd_in, int fd_out, char *remote_path, char *local_path,
    int pflag);

/*
 * Upload 'local_path' to 'remote_path'. Preserve permissions and times
 * if 'pflag' is set
 */
int do_upload(int fd_in, int fd_out, char *local_path, char *remote_path,
    int pflag);
