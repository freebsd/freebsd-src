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

#include "includes.h"
RCSID("$OpenBSD: sftp-glob.c,v 1.13 2002/09/11 22:41:50 djm Exp $");

#include "buffer.h"
#include "bufaux.h"
#include "xmalloc.h"
#include "log.h"

#include "sftp.h"
#include "sftp-common.h"
#include "sftp-client.h"
#include "sftp-glob.h"

struct SFTP_OPENDIR {
	SFTP_DIRENT **dir;
	int offset;
};

static struct {
	struct sftp_conn *conn;
} cur;

static void *
fudge_opendir(const char *path)
{
	struct SFTP_OPENDIR *r;

	r = xmalloc(sizeof(*r));

	if (do_readdir(cur.conn, (char *)path, &r->dir)) {
		xfree(r);
		return(NULL);
	}

	r->offset = 0;

	return((void *)r);
}

static struct dirent *
fudge_readdir(struct SFTP_OPENDIR *od)
{
	/* Solaris needs sizeof(dirent) + path length (see below) */
	static char buf[sizeof(struct dirent) + MAXPATHLEN];
	struct dirent *ret = (struct dirent *)buf;
#ifdef __GNU_LIBRARY__
	static int inum = 1;
#endif /* __GNU_LIBRARY__ */
	
	if (od->dir[od->offset] == NULL)
		return(NULL);

	memset(buf, 0, sizeof(buf));

	/*
	 * Solaris defines dirent->d_name as a one byte array and expects
	 * you to hack around it.
	 */
#ifdef BROKEN_ONE_BYTE_DIRENT_D_NAME
	strlcpy(ret->d_name, od->dir[od->offset++]->filename, MAXPATHLEN);
#else
	strlcpy(ret->d_name, od->dir[od->offset++]->filename,
	    sizeof(ret->d_name));
#endif
#ifdef __GNU_LIBRARY__
	/*
	 * Idiot glibc uses extensions to struct dirent for readdir with
	 * ALTDIRFUNCs. Not that this is documented anywhere but the 
	 * source... Fake an inode number to appease it.
	 */
	ret->d_ino = inum++;
	if (!inum)
		inum = 1;
#endif /* __GNU_LIBRARY__ */

	return(ret);
}

static void
fudge_closedir(struct SFTP_OPENDIR *od)
{
	free_sftp_dirents(od->dir);
	xfree(od);
}

static int
fudge_lstat(const char *path, struct stat *st)
{
	Attrib *a;

	if (!(a = do_lstat(cur.conn, (char *)path, 0)))
		return(-1);

	attrib_to_stat(a, st);

	return(0);
}

static int
fudge_stat(const char *path, struct stat *st)
{
	Attrib *a;

	if (!(a = do_stat(cur.conn, (char *)path, 0)))
		return(-1);

	attrib_to_stat(a, st);

	return(0);
}

int
remote_glob(struct sftp_conn *conn, const char *pattern, int flags,
    int (*errfunc)(const char *, int), glob_t *pglob)
{
	pglob->gl_opendir = fudge_opendir;
	pglob->gl_readdir = (struct dirent *(*)(void *))fudge_readdir;
	pglob->gl_closedir = (void (*)(void *))fudge_closedir;
	pglob->gl_lstat = fudge_lstat;
	pglob->gl_stat = fudge_stat;

	memset(&cur, 0, sizeof(cur));
	cur.conn = conn;

	return(glob(pattern, flags | GLOB_ALTDIRFUNC, errfunc, pglob));
}
