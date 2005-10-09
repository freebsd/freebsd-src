/*
 * Copyright (c) 2004 Alfred Perlstein <alfred@FreeBSD.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 * $Id: libautofs.c,v 1.5 2004/09/08 08:44:12 bright Exp $
 */
#include <err.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#ifdef AUTOFSSTANDALONE
#include "../autofs/autofs.h"
#else
#include <fs/autofs/autofs.h>
#endif

#include "libautofs.h"

struct auto_handle {
	char	ah_mp[MNAMELEN];
	fsid_t	ah_fsid;
	int	ah_fd;
};

static int	autofs_sysctl(int, fsid_t *, void *, size_t *, void *, size_t);
static void	safe_free(void *ptr);
static int	getmntlst(struct statfs **sfsp, int *cntp);

static void
safe_free(void *ptr)
{
	int saved_errno;

	saved_errno = errno;
	free(ptr);
	errno = saved_errno;
}

int
getmntlst(struct statfs **sfsp, int *cntp)
{
	int cnt;
	long bufsize;

	*sfsp = NULL;
	cnt = getfsstat(NULL, 0, MNT_NOWAIT);
	bufsize = cnt * sizeof(**sfsp);
	/*fprintf(stderr, "getmntlst bufsize %ld, cnt %d\n", bufsize, cnt);*/
	*sfsp = malloc(bufsize);
	if (sfsp == NULL)
		goto err;
	cnt = getfsstat(*sfsp, bufsize, MNT_NOWAIT);
	if (cnt == -1)
		goto err;
	*cntp = cnt;
	/*fprintf(stderr, "getmntlst ok, cnt %d\n", cnt);*/
	return (0);
err:
	safe_free(sfsp);
	*sfsp = NULL;
	/*fprintf(stderr, "getmntlst bad\n");*/
	return (-1);
}

/* get a handle based on a path. */
int
autoh_get(const char *path, autoh_t *ahp)
{
	struct statfs *sfsp, *sp;
	int cnt, fd, i;
	autoh_t ret;

	ret = NULL;
	/*
	 * We use getfsstat to prevent avoid the lookups on the mountpoints
	 * that statfs(2) would do.
	 */
	if (getmntlst(&sfsp, &cnt))
		goto err;
	for (i = 0; i < cnt; i++) {
		if (strcmp(sfsp[i].f_mntonname, path) == 0)
			break;
	}
	if (i == cnt) {
		/*fprintf(stderr, "autoh_get bad %d %d\n", i, cnt);*/
		errno = ENOENT;
		goto err;
	}
	sp = &sfsp[i];
	if (strcmp(sp->f_fstypename, "autofs")) {
		errno = ENOTTY;
		goto err;
	}
	fd = open(sp->f_mntonname, O_RDONLY);
	if (fd == -1)
		goto err;
	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		goto err;

	ret->ah_fsid = sp->f_fsid;
	ret->ah_fd = fd;
	strlcpy(ret->ah_mp, sp->f_mntonname, sizeof(ret->ah_mp));
	safe_free(sfsp);
	*ahp = ret;
	return (0);
err:
	safe_free(ret);
	safe_free(sfsp);
	return (-1);
}

/* release. */
void
autoh_free(autoh_t ah)
{
	int saved_errno;

	saved_errno = errno;
	close(ah->ah_fd);
	free(ah);
	errno = saved_errno;
}

/*
 * Get an array of pointers to all the currently mounted autofs
 * instances.
 */
int
autoh_getall(autoh_t **arrayp, int *cntp)
{
	struct statfs *sfsp;
	int cnt, i, pos;
	autoh_t *array;

	array = NULL;
	/*
	 * We use getfsstat to prevent avoid the lookups on the mountpoints
	 * that statfs(2) would do.
	 */
	if (getmntlst(&sfsp, &cnt))
		goto err;
	array = *arrayp = calloc(cnt + 1, sizeof(**arrayp));
	if (array == NULL)
		goto err;
	for (i = 0, pos = 0; i < cnt; i++) {
		if (autoh_get(sfsp[i].f_mntonname, &array[pos]) == -1) {
			/* not an autofs entry, that's ok, otherwise bail */
			if (errno == ENOTTY)
				continue;
			goto err;
		}
		pos++;
	}
	if (pos == 0) {
		errno = ENOENT;
		goto err;
	}
	*arrayp = array;
	*cntp = pos;
	safe_free(sfsp);
	return (0);
err:
	safe_free(sfsp);
	if (array)
		autoh_freeall(array);
	return (-1);
}

/* release. */
void
autoh_freeall(autoh_t *ah)
{
	autoh_t *ahp;

	ahp = ah;

	while (*ahp != NULL) {
		autoh_free(*ahp);
		ahp++;
	}
	safe_free(ah);
}

/* return fd to select on. */
int
autoh_fd(autoh_t ah)
{

	return (ah->ah_fd);
}

const char *
autoh_mp(autoh_t ah)
{

	return (ah->ah_mp);
}

static int do_autoreq_get(autoh_t ah, autoreq_t *reqp, int *cntp);

/* get an array of pending requests */
int
autoreq_get(autoh_t ah, autoreq_t **reqpp, int *cntp)
{
	int cnt, i;
	autoreq_t req, *reqp;

	if (do_autoreq_get(ah, &req, &cnt))
		return (-1);

	reqp = calloc(cnt + 1, sizeof(*reqp));
	if (reqp == NULL) {
		safe_free(req);
		return (-1);
	}
	for (i = 0; i < cnt; i++)
		reqp[i] = &req[i];
	*reqpp = reqp;
	*cntp = cnt;
	return (0);
}

int
do_autoreq_get(autoh_t ah, autoreq_t *reqp, int *cntp)
{
	size_t olen;
	struct autofs_userreq *reqs;
	int cnt, error;
	int vers;

	vers = AUTOFS_PROTOVERS;

	error = 0;
	reqs = NULL;
	olen = 0;
	cnt = 0;
	error = autofs_sysctl(AUTOFS_CTL_GETREQS, &ah->ah_fsid, NULL, &olen,
	    &vers, sizeof(vers));
	if (error == -1)
		goto out;
	if (olen == 0)
		goto out;

	reqs = malloc(olen);
	if (reqs == NULL)
		goto out;
	error = autofs_sysctl(AUTOFS_CTL_GETREQS, &ah->ah_fsid, reqs, &olen,
	    &vers, sizeof(vers));
	if (error == -1)
		goto out;
out:
	if (error) {
		safe_free(reqs);
		return (-1);
	}
	cnt = olen / sizeof(*reqs);
	*cntp = cnt;
	*reqp = reqs;
	return (0);
}

/* free an array of requests */
void
autoreq_free(autoh_t ah __unused, autoreq_t *req)
{

	free(*req);
	free(req);
}

/* serve a request */
int
autoreq_serv(autoh_t ah, autoreq_t req)
{
	int error;

	error = autofs_sysctl(AUTOFS_CTL_SERVREQ, &ah->ah_fsid, NULL, NULL,
	    req, sizeof(*req));
	return (error);
}

enum autoreq_op
autoreq_getop(autoreq_t req)
{

	switch (req->au_op) {
	case AREQ_LOOKUP:
		return (AUTOREQ_OP_LOOKUP);
	case AREQ_STAT:
		return (AUTOREQ_OP_STAT);
	case AREQ_READDIR:
		return (AUTOREQ_OP_READDIR);
	default:
		return (AUTOREQ_OP_UNKNOWN);
	}
}

/* get a request's file name. */
const char	*
autoreq_getpath(autoreq_t req)
{

	return (req->au_name);
}

/* get a request's inode.  a indirect mount may return AUTO_INODE_NONE. */
autoino_t
autoreq_getino(autoreq_t req)
{

	return (req->au_ino);
}

void
autoreq_setino(autoreq_t req, autoino_t ino)
{

	req->au_ino = ino;
}

/* get a request's directory inode. */
autoino_t
autoreq_getdirino(autoreq_t req)
{

	return (req->au_dino);
}

void
autoreq_seterrno(autoreq_t req, int error)
{

	req->au_errno = error;
}

void
autoreq_setaux(autoreq_t req, void *auxdata, size_t auxlen)
{

	req->au_auxdata = auxdata;
	req->au_auxlen = auxlen;
}

void
autoreq_getaux(autoreq_t req, void **auxdatap, size_t *auxlenp)
{

	*auxdatap = req->au_auxdata;
	*auxlenp = req->au_auxlen;
}

void
autoreq_seteof(autoreq_t req, int eof)
{

	req->au_eofflag = eof;
}

void
autoreq_getoffset(autoreq_t req, off_t *offp)
{

	*offp = req->au_offset - AUTOFS_USEROFF;
}

void
autoreq_getxid(autoreq_t req, int *xid)
{

	*xid = req->au_xid;
}

/* toggle by path. args = handle, AUTO_?, pid (-1 to disable), path. */
int
autoh_togglepath(autoh_t ah, int op, pid_t pid,  const char *path)
{
	int fd, ret;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return (-1);
	ret = autoh_togglefd(ah, op, pid, fd);
	close(fd);
	return (ret);
}

/* toggle by fd. args = handle, AUTO_?, pid (-1 to disable), fd. */
int
autoh_togglefd(autoh_t ah, int op, pid_t pid, int fd)
{
	struct stat sb;
	struct autofs_mounterreq mr;
	int error, realop;

	switch (op) {
	case AUTO_DIRECT:
		realop = AUTOFS_CTL_TRIGGER;
		break;
	case AUTO_INDIRECT:
		realop = AUTOFS_CTL_SUBTRIGGER;
		break;
	case AUTO_MOUNTER:
		realop = AUTOFS_CTL_MOUNTER;
		break;
	case AUTO_BROWSE:
		realop = AUTOFS_CTL_BROWSE;
		break;
	default:
		errno = ENOTTY;
		return (-1);
	}

	if (fstat(fd, &sb))
		return (-1);
	bzero(&mr, sizeof(mr));
	mr.amu_ino = sb.st_ino;
	mr.amu_pid = pid;
	error = autofs_sysctl(realop, &ah->ah_fsid, NULL, NULL,
	    &mr, sizeof(mr));
	return (error);
}

int
autofs_sysctl(op, fsid, oldp, oldlenp, newp, newlen)
	int op;
	fsid_t *fsid;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	struct vfsidctl vc;

	bzero(&vc, sizeof(vc));
	vc.vc_op = op;
	strcpy(vc.vc_fstypename, "*");
	vc.vc_vers = VFS_CTL_VERS1;
	vc.vc_fsid = *fsid;
	vc.vc_ptr = newp;
	vc.vc_len = newlen;
	return (sysctlbyname("vfs.autofs.ctl", oldp, oldlenp, &vc, sizeof(vc)));
}

