/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Based on libixp code: ©2007-2010 Kris Maglione <maglione.k at Gmail>
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include "../lib9p.h"
#include "../lib9p_impl.h"
#include "../log.h"
#include "../rfuncs.h"

#if defined(__FreeBSD__)
  #include <sys/param.h>
  #if __FreeBSD_version >= 1000000
    #define	HAVE_BINDAT
  #endif
#endif

#if defined(__FreeBSD__)
  #define	HAVE_BIRTHTIME
#endif

#if defined(__FreeBSD__)
  /* should probably check version but fstatat has been in for ages */
  #define HAVE_FSTATAT
#endif

#if defined(__APPLE__)
  #include "Availability.h"
  #if __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    #define HAVE_FSTATAT
  #endif
#endif

static struct openfile *open_fid(const char *);
static void dostat(struct l9p_stat *, char *, struct stat *, bool dotu);
static void dostatfs(struct l9p_statfs *, struct statfs *, long);
static void generate_qid(struct stat *, struct l9p_qid *);
static void fs_attach(void *, struct l9p_request *);
static void fs_clunk(void *, struct l9p_request *);
static void fs_create(void *, struct l9p_request *);
static void fs_flush(void *, struct l9p_request *);
static void fs_open(void *, struct l9p_request *);
static void fs_read(void *, struct l9p_request *);
static void fs_remove(void *, struct l9p_request *);
static void fs_stat(void *, struct l9p_request *);
static void fs_walk(void *, struct l9p_request *);
static void fs_write(void *, struct l9p_request *);
static void fs_wstat(void *, struct l9p_request *);
static void fs_statfs(void *, struct l9p_request *);
static void fs_lopen(void *, struct l9p_request *);
static void fs_lcreate(void *, struct l9p_request *);
static void fs_symlink(void *, struct l9p_request *);
static void fs_mknod(void *, struct l9p_request *);
static void fs_rename(void *, struct l9p_request *);
static void fs_readlink(void *, struct l9p_request *);
static void fs_getattr(void *, struct l9p_request *);
static void fs_setattr(void *, struct l9p_request *);
static void fs_xattrwalk(void *, struct l9p_request *);
static void fs_xattrcreate(void *, struct l9p_request *);
static void fs_readdir(void *, struct l9p_request *);
static void fs_fsync(void *, struct l9p_request *);
static void fs_lock(void *, struct l9p_request *);
static void fs_getlock(void *, struct l9p_request *);
static void fs_link(void *, struct l9p_request *);
static void fs_renameat(void *softc, struct l9p_request *req);
static void fs_unlinkat(void *softc, struct l9p_request *req);
static void fs_freefid(void *softc, struct l9p_openfile *f);

struct fs_softc {
	const char *fs_rootpath;
	bool fs_readonly;
	TAILQ_HEAD(, fs_tree) fs_auxtrees;
};

struct fs_tree {
	const char *fst_name;
	const char *fst_path;
	bool fst_readonly;
	TAILQ_ENTRY(fs_tree) fst_link;
};

struct openfile {
	DIR *dir;
	int fd;
	char *name;
	uid_t uid;
	gid_t gid;
};

static struct openfile *
open_fid(const char *path)
{
	struct openfile *ret;

	ret = l9p_calloc(1, sizeof(*ret));
	ret->fd = -1;
	ret->name = strdup(path);
	return (ret);
}

static void
dostat(struct l9p_stat *s, char *name, struct stat *buf, bool dotu)
{
	struct passwd *user;
	struct group *group;

	memset(s, 0, sizeof(struct l9p_stat));

	generate_qid(buf, &s->qid);

	s->type = 0;
	s->dev = 0;
	s->mode = buf->st_mode & 0777;

	if (S_ISDIR(buf->st_mode))
		s->mode |= L9P_DMDIR;

	if (S_ISLNK(buf->st_mode) && dotu)
		s->mode |= L9P_DMSYMLINK;

	if (S_ISCHR(buf->st_mode) || S_ISBLK(buf->st_mode))
		s->mode |= L9P_DMDEVICE;

	if (S_ISSOCK(buf->st_mode))
		s->mode |= L9P_DMSOCKET;

	if (S_ISFIFO(buf->st_mode))
		s->mode |= L9P_DMNAMEDPIPE;

	s->atime = (uint32_t)buf->st_atime;
	s->mtime = (uint32_t)buf->st_mtime;
	s->length = (uint64_t)buf->st_size;

	s->name = r_basename(name, NULL, 0);

	if (!dotu) {
		struct r_pgdata udata, gdata;

		user = r_getpwuid(buf->st_uid, &udata);
		group = r_getgrgid(buf->st_gid, &gdata);
		s->uid = user != NULL ? strdup(user->pw_name) : NULL;
		s->gid = group != NULL ? strdup(group->gr_name) : NULL;
		s->muid = user != NULL ? strdup(user->pw_name) : NULL;
		r_pgfree(&udata);
		r_pgfree(&gdata);
	} else {
		/*
		 * When using 9P2000.u, we don't need to bother about
		 * providing user and group names in textual form.
		 *
		 * NB: if the asprintf()s fail, s->extension should
		 * be unset so we can ignore these.
		 */
		s->n_uid = buf->st_uid;
		s->n_gid = buf->st_gid;
		s->n_muid = buf->st_uid;

		if (S_ISLNK(buf->st_mode)) {
			char target[MAXPATHLEN];
			ssize_t ret = readlink(name, target, MAXPATHLEN);

			if (ret < 0) {
				s->extension = NULL;
				return;
			}

			s->extension = strndup(target, (size_t)ret);
		}

		if (S_ISBLK(buf->st_mode)) {
			asprintf(&s->extension, "b %d %d", major(buf->st_rdev),
			    minor(buf->st_rdev));
		}

		if (S_ISCHR(buf->st_mode)) {
			asprintf(&s->extension, "c %d %d", major(buf->st_rdev),
			    minor(buf->st_rdev));
		}
	}
}

static void dostatfs(struct l9p_statfs *out, struct statfs *in, long namelen)
{

	out->type = 0;		/* XXX */
	out->bsize = in->f_bsize;
	out->blocks = in->f_blocks;
	out->bfree = in->f_bfree;
	out->bavail = in->f_bavail;
	out->files = in->f_files;
	out->ffree = in->f_ffree;
	out->fsid = ((uint64_t)in->f_fsid.val[0] << 32) | (uint64_t)in->f_fsid.val[1];
	out->namelen = (uint32_t)namelen;
}

static void
generate_qid(struct stat *buf, struct l9p_qid *qid)
{
	qid->path = buf->st_ino;
	qid->version = 0;

	if (S_ISREG(buf->st_mode))
		qid->type |= L9P_QTFILE;

	if (S_ISDIR(buf->st_mode))
		qid->type |= L9P_QTDIR;

	if (S_ISLNK(buf->st_mode))
		qid->type |= L9P_QTSYMLINK;
}

static bool
check_access(struct stat *st, uid_t uid, int amode)
{
	struct passwd *pwd;
#ifdef __FreeBSD__	/* XXX need better way to determine this */
	gid_t groups[NGROUPS_MAX];
#else
	int groups[NGROUPS_MAX];
#endif
	int ngroups = NGROUPS_MAX;
	int i, mask;

	if (uid == 0)
		return (true);

	/*
	 * This is a bit dirty (using the well known mode bits instead
	 * of the S_I[RWX]{USR,GRP,OTH} macros), but lets us be very
	 * efficient about it.
	 */
	switch (amode) {
	case L9P_ORDWR:
		mask = 0600;
		break;
	case L9P_OREAD:
		mask = 0400;
		break;
	case L9P_OWRITE:
		mask = 0200;
		break;
	case L9P_OEXEC:
		mask = 0100;
		break;
	default:
		/* probably should not even get here */
		return (false);
	}

	/*
	 * Normal Unix semantics are: apply user permissions first
	 * and if these fail, reject the request entirely.  Current
	 * lib9p semantics go on to allow group or other as well.
	 *
	 * Also, we check "other" before "group" because group check
	 * is expensive.  (Perhaps should cache uid -> gid mappings?)
	 */
	if (st->st_uid == uid) {
		if ((st->st_mode & mask) == mask)
			return (true);
	}

	/* Check for "other" access */
	mask >>= 6;
	if ((st->st_mode & mask) == mask)
		return (true);

	/* Check for group access - XXX: not thread safe */
	mask <<= 3;
	pwd = getpwuid(uid);
	getgrouplist(pwd->pw_name, (int)pwd->pw_gid, groups, &ngroups);

	for (i = 0; i < ngroups; i++) {
		if (st->st_gid == (gid_t)groups[i]) {
			if ((st->st_mode & mask) == mask)
				return (true);
		}
	}

	return (false);
}

static void
fs_attach(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = (struct fs_softc *)softc;
	struct openfile *file;
	struct passwd *pwd;
	uid_t uid;

	assert(req->lr_fid != NULL);

	file = open_fid(sc->fs_rootpath);
	req->lr_fid->lo_qid.type = L9P_QTDIR;
	req->lr_fid->lo_qid.path = (uintptr_t)req->lr_fid;
	req->lr_fid->lo_aux = file;
	req->lr_resp.rattach.qid = req->lr_fid->lo_qid;

	uid = req->lr_req.tattach.n_uname;
	if (req->lr_conn->lc_version >= L9P_2000U && uid != (uid_t)-1)
		pwd = getpwuid(uid);
	else
		pwd = getpwnam(req->lr_req.tattach.uname);

	if (pwd == NULL) {
		l9p_respond(req, EPERM);
		return;
	}

	file->uid = pwd->pw_uid;
	file->gid = pwd->pw_gid;
	l9p_respond(req, 0);
}

static void
fs_clunk(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;

	file = req->lr_fid->lo_aux;
	assert(file != NULL);

	if (file->dir) {
		closedir(file->dir);
		file->dir = NULL;
	} else if (file->fd != -1) {
		close(file->fd);
		file->fd = -1;
	}

	l9p_respond(req, 0);
}

/*
 * Internal helpers for create ops.
 *
 * Currently these are mostly trivial since this is meant to be
 * semantically identical to the previous version of the code, but
 * they will be modified to handle additional details correctly in
 * a subsequent commit.
 */
static inline int
internal_mkdir(char *newname, mode_t mode, struct stat *st)
{

	/*
	 * https://swtch.com/plan9port/man/man9/open.html
	 * says that permissions are actually
	 * perm & (~0777 | (dir.perm & 0777)).
	 * This seems a bit restrictive; probably
	 * there should be a control knob for this.
	 */
	mode &= (~0777 | (st->st_mode & 0777));
	if (mkdir(newname, mode) != 0)
		return (errno);
	return (0);
}

static inline int
internal_symlink(char *symtgt, char *newname)
{

	if (symlink(symtgt, newname) != 0)
		return (errno);
	return (0);
}

static inline int
internal_mkfifo(char *newname, mode_t mode)
{

	if (mkfifo(newname, mode) != 0)
		return (errno);
	return (0);
}


static inline int
internal_mksocket(struct openfile *file __unused, char *newname,
    char *reqname __unused)
{
	struct sockaddr_un sun;
	char *path;
	int error = 0;
	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	int fd;

	if (s < 0)
		return (errno);

	path = newname;
	fd = -1;
#ifdef HAVE_BINDAT
	/* Try bindat() if needed. */
	if (strlen(path) >= sizeof(sun.sun_path)) {
		fd = open(file->name, O_RDONLY);
		if (fd >= 0)
			path = reqname;
	}
#endif

	/*
	 * Can only create the socket if the path will fit.
	 * Even if we are using bindat() there are limits
	 * (the API for AF_UNIX sockets is ... not good).
	 *
	 * Note: in theory we can fill sun_path to the end
	 * (omitting a terminating '\0') but in at least one
	 * Unix-like system, this was known to behave oddly,
	 * so we test for ">=" rather than just ">".
	 */
	if (strlen(path) >= sizeof(sun.sun_path)) {
		error = ENAMETOOLONG;
		goto out;
	}
	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof(struct sockaddr_un);
	strncpy(sun.sun_path, path, sizeof(sun.sun_path));

#ifdef HAVE_BINDAT
	if (fd >= 0) {
		if (bindat(fd, s, (struct sockaddr *)&sun, sun.sun_len) < 0)
			error = errno;
		goto out;	/* done now, for good or ill */
	}
#endif

	if (bind(s, (struct sockaddr *)&sun, sun.sun_len) < 0)
		error = errno;
out:

	/*
	 * It's not clear which error should override, although
	 * ideally we should never see either close() call fail.
	 * In any case we do want to try to close both fd and s,
	 * always.  Let's set error only if it is not already set,
	 * so that all exit paths can use the same code.
	 */
	if (fd >= 0 && close(fd) != 0)
		if (error == 0)
			error = errno;
	if (close(s) != 0)
		if (error == 0)
			error = errno;

	return (error);
}

static inline int
internal_mknod(struct l9p_request *req, char *newname, mode_t mode)
{
	char type;
	unsigned int major, minor;

	/*
	 * ??? Should this be testing < 3?  For now, allow a single
	 * integer mode with minor==0 implied.
	 */
	minor = 0;
	if (sscanf(req->lr_req.tcreate.extension, "%c %u %u",
	    &type, &major, &minor) < 2) {
		return (EINVAL);
	}

	switch (type) {
	case 'b':
		mode |= S_IFBLK;
		break;
	case 'c':
		mode |= S_IFCHR;
		break;
	default:
		return (EINVAL);
	}
	if (mknod(newname, mode, makedev(major, minor)) != 0)
		return (errno);
	return (0);
}

/*
 * Create ops.
 *
 * We are to create a new file under some existing path,
 * where the new file's name is in the Tcreate request and the
 * existing path is due to a fid-based file (req->lr_fid_lo_aux).
 *
 * Some ops (regular open) set file->fd, most do not.
 */
static void
fs_create(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file = req->lr_fid->lo_aux;
	struct stat st;
	char *newname = NULL;
	mode_t mode = req->lr_req.tcreate.perm & 0777;
	int error = 0;

	assert(file != NULL);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	/*
	 * Build the full path.  We may not use it in some cases
	 * (e.g., when we can use the *at() calls), and perhaps
	 * should defer building it, but this suffices for now,
	 * and gives us a place to put a max on path name length
	 * (perhaps we should check even if asprintf() succeeds?).
	 */
	if (asprintf(&newname, "%s/%s",
	    file->name, req->lr_req.tcreate.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}

	/*
	 * Containing directory must exist and allow access.
	 *
	 * There is a race here between test and subsequent
	 * operation, which we cannot close in general, but
	 * ideally, no one should be changing things underneath
	 * us.  It might therefore also be nice to keep cached
	 * lstat data, but we leave that to future optimization
	 * (after profiling).
	 */
	if (lstat(file->name, &st) != 0) {
		error = errno;
		goto out;
	}

	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}

	if (req->lr_req.tcreate.perm & L9P_DMDIR)
		error = internal_mkdir(newname, mode, &st);
	else if (req->lr_req.tcreate.perm & L9P_DMSYMLINK)
		error = internal_symlink(req->lr_req.tcreate.extension,
		    newname);
	else if (req->lr_req.tcreate.perm & L9P_DMNAMEDPIPE)
		error = internal_mkfifo(newname, mode);
	else if (req->lr_req.tcreate.perm & L9P_DMSOCKET)
		error = internal_mksocket(file, newname,
		    req->lr_req.tcreate.name);
	else if (req->lr_req.tcreate.perm & L9P_DMDEVICE)
		error = internal_mknod(req, newname, mode);
	else {
		/*
		 * https://swtch.com/plan9port/man/man9/open.html
		 * says that permissions are actually
		 * perm & (~0666 | (dir.perm & 0666)).
		 * This seems a bit restrictive; probably
		 * there should be a control knob for this.
		 */
		mode &= (~0666 | st.st_mode) & 0666;
		file->fd = open(newname,
		    O_CREAT | O_TRUNC | req->lr_req.tcreate.mode,
		    mode);
		if (file->fd < 0)
			error = errno;
	}

	if (error)
		goto out;

	if (lchown(newname, file->uid, file->gid) != 0) {
		error = errno;
		goto out;
	}

	if (lstat(newname, &st) != 0) {
		error = errno;
		goto out;
	}

	generate_qid(&st, &req->lr_resp.rcreate.qid);
out:
	free(newname);
	l9p_respond(req, error);
}

static void
fs_flush(void *softc __unused, struct l9p_request *req)
{

	/* XXX: not used because this transport is synchronous */
	l9p_respond(req, 0);
}

static void
fs_open(void *softc __unused, struct l9p_request *req)
{
	struct l9p_connection *conn = req->lr_conn;
	struct openfile *file = req->lr_fid->lo_aux;
	struct stat st;

	assert(file != NULL);

	if (stat(file->name, &st) != 0) {
		l9p_respond(req, errno);
		return;
	}

	if (!check_access(&st, file->uid, req->lr_req.topen.mode)) {
		l9p_respond(req, EPERM);
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		file->dir = opendir(file->name);
		if (file->dir == NULL) {
			l9p_respond(req, EPERM);	/* ??? */
			return;
		}
	} else {
		file->fd = open(file->name, req->lr_req.topen.mode);
		if (file->fd < 0) {
			l9p_respond(req, EPERM);
			return;
		}
	}

	generate_qid(&st, &req->lr_resp.ropen.qid);

	req->lr_resp.ropen.iounit = conn->lc_max_io_size;
	l9p_respond(req, 0);
}

/*
 * Helper for directory read.  We want to run an lstat on each
 * file name within the directory.  This is a lot faster if we
 * have lstatat (or fstatat with AT_SYMLINK_NOFOLLOW), but not
 * all systems do, so hide the ifdef-ed code in an inline function.
 */
static inline int
fs_lstatat(struct openfile *file, char *name, struct stat *st)
{
#ifdef HAVE_FSTATAT
	return (fstatat(dirfd(file->dir), name, st, AT_SYMLINK_NOFOLLOW));
#else
	char buf[MAXPATHLEN];

	if (strlcpy(buf, file->name, sizeof(buf)) >= sizeof(buf) ||
	    strlcat(buf, name, sizeof(buf)) >= sizeof(buf))
		return (-1);
	return (lstat(name, st));
#endif
}

static void
fs_read(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;
	struct l9p_stat l9stat;
	bool dotu = req->lr_conn->lc_version >= L9P_2000U;
	ssize_t ret;

	file = req->lr_fid->lo_aux;
	assert(file != NULL);

	if (file->dir != NULL) {
		struct dirent *d;
		struct stat st;
		struct l9p_message msg;
		long o;

		/*
		 * Must use telldir before readdir since seekdir
		 * takes cookie values.  Unfortunately this wastes
		 * a lot of time (and memory) building unneeded
		 * cookies that can only be flushed by closing
		 * the directory.
		 *
		 * NB: FreeBSD libc seekdir has SINGLEUSE defined,
		 * so in fact, we can discard the cookies by
		 * calling seekdir on them.  This clears up wasted
		 * memory at the cost of even more wasted time...
		 *
		 * XXX: readdir/telldir/seekdir not thread safe
		 */
		l9p_init_msg(&msg, req, L9P_PACK);
		for (;;) {
			o = telldir(file->dir);
			d = readdir(file->dir);
			if (d == NULL)
				break;
			if (fs_lstatat(file, d->d_name, &st))
				continue;
			dostat(&l9stat, d->d_name, &st, dotu);
			if (l9p_pack_stat(&msg, req, &l9stat) != 0) {
				seekdir(file->dir, o);
				break;
			}
#if defined(__FreeBSD__)
			seekdir(file->dir, o);
			(void) readdir(file->dir);
#endif
		}
	} else {
		size_t niov = l9p_truncate_iov(req->lr_data_iov,
                    req->lr_data_niov, req->lr_req.io.count);

#if defined(__FreeBSD__)
		ret = preadv(file->fd, req->lr_data_iov, niov,
		    req->lr_req.io.offset);
#else
		/* XXX: not thread safe, should really use aio_listio. */
		if (lseek(file->fd, (off_t)req->lr_req.io.offset, SEEK_SET) < 0)
		{
			l9p_respond(req, errno);
			return;
		}

		ret = (uint32_t)readv(file->fd, req->lr_data_iov, (int)niov);
#endif

		if (ret < 0) {
			l9p_respond(req, errno);
			return;
		}

		req->lr_resp.io.count = (uint32_t)ret;
	}

	l9p_respond(req, 0);
}

static void
fs_remove(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct stat st;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (sc->fs_readonly) {
		l9p_respond(req, EROFS);
		return;
	}

	if (lstat(file->name, &st) != 0) {
		l9p_respond(req, errno);
		return;
	}

	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		l9p_respond(req, EPERM);
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		if (rmdir(file->name) != 0) {
			l9p_respond(req, errno);
			return;
		}
	} else {
		if (unlink(file->name) != 0) {
			l9p_respond(req, errno);
			return;
		}
	}

	l9p_respond(req, 0);
}

static void
fs_stat(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;
	struct stat st;
	bool dotu = req->lr_conn->lc_version >= L9P_2000U;

	file = req->lr_fid->lo_aux;
	assert(file);

	lstat(file->name, &st);
	dostat(&req->lr_resp.rstat.stat, file->name, &st, dotu);

	l9p_respond(req, 0);
}

static void
fs_walk(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct stat st;
	struct openfile *file = req->lr_fid->lo_aux;
	struct openfile *newfile;
	size_t clen, namelen, need;
	char *comp, *succ, *next, *swtmp;
	bool dotdot;
	int i, nwname;
	int error = 0;
	char namebufs[2][MAXPATHLEN];

	/*
	 * https://swtch.com/plan9port/man/man9/walk.html:
	 *
	 *    It is legal for nwname to be zero, in which case newfid
	 *    will represent the same file as fid and the walk will
	 *    usually succeed; this is equivalent to walking to dot.
	 * [Aside: it's not clear if we should test S_ISDIR here.]
	 *    ...
	 *    The name ".." ... represents the parent directory.
	 *    The name "." ... is not used in the protocol.
	 *    ... A walk of the name ".." in the root directory
	 *    of the server is equivalent to a walk with no name
	 *    elements.
	 *
	 * Note that req.twalk.nwname never exceeds L9P_MAX_WELEM,
	 * so it is safe to convert to plain int.
	 *
	 * We are to return an error only if the first walk fails,
	 * else stop at the end of the names or on the first error.
	 * The final fid is based on the last name successfully
	 * walked.
	 *
	 * Set up "successful name" buffer pointer with base fid name,
	 * initially.  We'll swap each new success into it as we go.
	 */
	succ = namebufs[0];
	next = namebufs[1];
	namelen = strlcpy(succ, file->name, MAXPATHLEN);
	if (namelen >= MAXPATHLEN) {
		error = ENAMETOOLONG;
		goto out;
	}
	if (lstat(succ, &st) < 0){
		error = errno;
		goto out;
	}
	nwname = (int)req->lr_req.twalk.nwname;
	/*
	 * Must have execute permission to search a directory.
	 * Then, look up each component in the directory.
	 * Check for ".." along the way, handlng specially
	 * as needed.  Forbid "/" in name components.
	 *
	 * Note that we *do* get Twalk requests with
	 * nwname==0 on files.
	 */
	if (nwname > 0) {
		if (!S_ISDIR(st.st_mode)) {
			error = ENOTDIR;
			goto out;
		}
		if (!check_access(&st, file->uid, L9P_OEXEC)) {
			L9P_LOG(L9P_DEBUG,
			    "Twalk: denying dir-walk on \"%s\" for uid %u",
			    succ, (unsigned)file->uid);
			error = EPERM;
			goto out;
		}
	}
	for (i = 0; i < nwname; i++) {
		comp = req->lr_req.twalk.wname[i];
		if (strchr(comp, '/') != NULL) {
			error = EINVAL;
			break;
		}
		clen = strlen(comp);
		dotdot = false;
		if (comp[0] == '.') {
			if (clen == 1) {
				error = EINVAL;
				break;
			}
			if (comp[1] == '.' && clen == 2) {
				dotdot = true;
				/*
				 * It's not clear how ".." at
				 * root should be handled if i > 0.
				 * Obeying the man page exactly, we
				 * reset i to 0 and stop, declaring
				 * terminal success.
				 */
				if (strcmp(file->name, sc->fs_rootpath) == 0) {
					succ = file->name;
					i = 0;
					break;
				}
			}
		}
		/*
		 * Build next pathname (into "next").  If dotdot,
		 * just strip one name component off file->name.
		 * Since we know file->name fits, the stripped down
		 * version also fits.  Otherwise, the name is the
		 * base name plus '/' plus the component name
		 * plus terminating '\0'; this may or may not
		 * fit.
		 */
		if (dotdot) {
			(void) r_dirname(file->name, next, MAXPATHLEN);
		} else {
			need = namelen + 1 + clen + 1;
			if (need > MAXPATHLEN) {
				error = ENAMETOOLONG;
				break;
			}
			memcpy(next, file->name, namelen);
			next[namelen] = '/';
			memcpy(&next[namelen + 1], comp, clen + 1);
		}
		if (lstat(next, &st) < 0){
			error = ENOENT;
			break;
		}
		/*
		 * Success: generate qid and swap this
		 * successful name into place.
		 */
		generate_qid(&st, &req->lr_resp.rwalk.wqid[i]);
		swtmp = succ;
		succ = next;
		next = swtmp;
	}

	/*
	 * Fail only if we failed on the first name.
	 * Otherwise we succeeded on something, and "succ"
	 * points to the last successful name in namebufs[].
	 */
	if (error) {
		if (i == 0)
			goto out;
		error = 0;
	}

	newfile = open_fid(succ);
	if (newfile == NULL) {
		error = ENOMEM;
		goto out;
	}
	newfile->uid = file->uid;
	newfile->gid = file->gid;
	req->lr_newfid->lo_aux = newfile;
	req->lr_resp.rwalk.nwqid = (uint16_t)i;
out:
	l9p_respond(req, error);
}

static void
fs_write(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	ssize_t ret;

	file = req->lr_fid->lo_aux;
	assert(file != NULL);

	if (sc->fs_readonly) {
		l9p_respond(req, EROFS);
		return;
	}

	size_t niov = l9p_truncate_iov(req->lr_data_iov,
            req->lr_data_niov, req->lr_req.io.count);

#if defined(__FreeBSD__)
	ret = pwritev(file->fd, req->lr_data_iov, niov,
	    req->lr_req.io.offset);
#else
	/* XXX: not thread safe, should really use aio_listio. */
	if (lseek(file->fd, (off_t)req->lr_req.io.offset, SEEK_SET) < 0) {
		l9p_respond(req, errno);
		return;
	}

	ret = writev(file->fd, req->lr_data_iov,
	    (int)niov);
#endif

	if (ret < 0) {
		l9p_respond(req, errno);
		return;
	}

	req->lr_resp.io.count = (uint32_t)ret;
	l9p_respond(req, 0);
}

static void
fs_wstat(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct l9p_stat *l9stat = &req->lr_req.twstat.stat;
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file != NULL);

	/*
	 * XXX:
	 *
	 * stat(9P) sez:
	 *
	 * Either all the changes in wstat request happen, or none of them
	 * does: if the request succeeds, all changes were made; if it fails,
	 * none were.
	 *
	 * Atomicity is clearly missing in current implementation.
	 */

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	if (l9stat->atime != (uint32_t)~0) {
		/* XXX: not implemented, ignore */
	}

	if (l9stat->mtime != (uint32_t)~0) {
		/* XXX: not implemented, ignore */
	}

	if (l9stat->dev != (uint32_t)~0) {
		error = EPERM;
		goto out;
	}

	if (l9stat->length != (uint64_t)~0) {
		if (file->dir != NULL) {
			error = EINVAL;
			goto out;
		}

		if (truncate(file->name, (off_t)l9stat->length) != 0) {
			error = errno;
			goto out;
		}
	}

	if (req->lr_conn->lc_version >= L9P_2000U) {
		if (lchown(file->name, l9stat->n_uid, l9stat->n_gid) != 0) {
			error = errno;
			goto out;
		}
	}

	if (l9stat->mode != (uint32_t)~0) {
		if (chmod(file->name, l9stat->mode & 0777) != 0) {
			error = errno;
			goto out;
		}
	}

	if (strlen(l9stat->name) > 0) {
		char *dir;
		char *newname;
		char *tmp;

		/* Forbid renaming root fid. */
		if (strcmp(file->name, sc->fs_rootpath) == 0) {
			error = EINVAL;
			goto out;
		}
		dir = r_dirname(file->name, NULL, 0);
		if (dir == NULL) {
			error = errno;
			goto out;
		}
		if (asprintf(&newname, "%s/%s", dir, l9stat->name) < 0) {
			error = errno;
			free(dir);
			goto out;
		}
		if (rename(file->name, newname))
			error = errno;
		else {
			/* Successful rename, update file->name. */
			tmp = newname;
			newname = file->name;
			file->name = tmp;
		}
		free(newname);
		free(dir);
	}
out:
	l9p_respond(req, error);
}

static void
fs_statfs(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;
	struct stat st;
	struct statfs f;
	long name_max;
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (lstat(file->name, &st) != 0) {
		error = errno;
		goto out;
	}

	if (!check_access(&st, file->uid, L9P_OREAD)) {
		error = EPERM;
		goto out;
	}

	if (statfs(file->name, &f) != 0) {
		error = errno;
		goto out;
	}

	name_max = pathconf(file->name, _PC_NAME_MAX);
	if (name_max == -1) {
		error = errno;
		goto out;
	}

	dostatfs(&req->lr_resp.rstatfs.statfs, &f, name_max);

out:
	l9p_respond(req, error);
}

/*
 * Linux O_* flag values do not match BSD ones.
 * It's not at all clear which flags Linux systems actually pass;
 * for now, we will just reject most.
 */
#define	L_O_CREAT	000000100U
#define	L_O_EXCL	000000200U
#define	L_O_TRUNC	000001000U
#define	L_O_APPEND	000002000U
#define	L_O_NONBLOCK	000004000U
#define	L_O_LARGEFILE	000100000U
#define	L_O_DIRECTORY	000200000U
#define	L_O_NOFOLLOW	000400000U
#define	L_O_TMPFILE	020000000U	/* ??? should we get this? */

#define	LO_LC_FORBID	(0xfffffffc & ~(L_O_CREAT | L_O_EXCL | L_O_TRUNC | \
					L_O_APPEND | L_O_NOFOLLOW | \
					L_O_DIRECTORY | L_O_LARGEFILE | \
					L_O_NONBLOCK))

/*
 * Common code for LOPEN and LCREATE requests.
 *
 * Note that the fid represents the containing directory for
 * LCREATE, and the file for LOPEN.  The newname argument is NULL
 * for LOPEN (and the mode and gid are 0).
 */
static int
fs_lo_lc(struct fs_softc *sc, struct l9p_request *req,
	char *newname, uint32_t lflags, uint32_t mode, uint32_t gid,
	struct stat *stp)
{
	struct openfile *file = req->lr_fid->lo_aux;
	int oflags, oacc;
	int resultfd;

	assert(file != NULL);

	/* low order bits match BSD; convert to L9P */
	switch (lflags & O_ACCMODE) {
	case O_RDONLY:
		oacc = L9P_OREAD;
		break;
	case O_WRONLY:
		oacc = L9P_OWRITE;
		break;
	case O_RDWR:
		oacc = L9P_ORDWR;
		break;
	default:
		return (EINVAL);
	}

	if (sc->fs_readonly && (newname || oacc != L9P_OREAD))
		return (EROFS);

	if (lflags & LO_LC_FORBID)
		return (ENOTSUP);

	/*
	 * What if anything should we do with O_NONBLOCK?
	 * (Currently we ignore it.)
	 */
	oflags = lflags & O_ACCMODE;
	if (lflags & L_O_CREAT)
		oflags |= O_CREAT;
	if (lflags & L_O_EXCL)
		oflags |= O_EXCL;
	if (lflags & L_O_TRUNC)
		oflags |= O_TRUNC;
	if (lflags & L_O_DIRECTORY)
		oflags |= O_DIRECTORY;
	if (lflags & L_O_APPEND)
		oflags |= O_APPEND;
	if (lflags & L_O_NOFOLLOW)
		oflags |= O_NOFOLLOW;

	if (newname == NULL) {
		/* open: require access, including write if O_TRUNC */
		if (lstat(file->name, stp) != 0)
			return (errno);
		if ((lflags & L_O_TRUNC) && oacc == L9P_OREAD)
			oacc = L9P_ORDWR;
		if (!check_access(stp, file->uid, oacc))
			return (EPERM);
		/* Cancel O_CREAT|O_EXCL since we are not creating. */
		oflags &= ~(O_CREAT | O_EXCL);
		if (S_ISDIR(stp->st_mode)) {
			/* Should we refuse if not O_DIRECTORY? */
			file->dir = opendir(file->name);
			if (file->dir == NULL)
				return (errno);
			resultfd = dirfd(file->dir);
		} else {
			file->fd = open(file->name, oflags);
			if (file->fd < 0)
				return (errno);
			resultfd = file->fd;
		}
	} else {
		/*
		 * XXX racy, see fs_create.
		 * Note, file->name is testing the containing dir,
		 * not the file itself (so O_CREAT is still OK).
		 */
		if (lstat(file->name, stp) != 0)
			return (errno);
		if (!check_access(stp, file->uid, L9P_OWRITE))
			return (EPERM);
		file->fd = open(newname, oflags, mode);
		if (file->fd < 0)
			return (errno);
		if (fchown(file->fd, file->uid, gid) != 0)
			return (errno);
		resultfd = file->fd;
	}

	if (fstat(resultfd, stp) != 0)
		return (errno);

	return (0);
}

static void
fs_lopen(void *softc, struct l9p_request *req)
{
	struct stat st;
	int error;

	error = fs_lo_lc(softc, req, NULL, req->lr_req.tlopen.flags, 0, 0, &st);
	if (error == 0) {
		generate_qid(&st, &req->lr_resp.rlopen.qid);
		req->lr_resp.rlopen.iounit = req->lr_conn->lc_max_io_size;
	}
	l9p_respond(req, error);
}

static void
fs_lcreate(void *softc, struct l9p_request *req)
{
	struct openfile *file = req->lr_fid->lo_aux;
	struct stat st;
	char *newname;
	int error;

	if (asprintf(&newname, "%s/%s",
	    file->name, req->lr_req.tlcreate.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}
	error = fs_lo_lc(softc, req, newname,
	    req->lr_req.tlcreate.flags, req->lr_req.tlcreate.mode,
	    req->lr_req.tlcreate.gid, &st);
	if (error == 0) {
		generate_qid(&st, &req->lr_resp.rlcreate.qid);
		req->lr_resp.rcreate.iounit = req->lr_conn->lc_max_io_size;
	}
	free(newname);
out:
	l9p_respond(req, error);
}

/*
 * Could use a bit more work to reduce code duplication with fs_create.
 */
static void
fs_symlink(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct stat st;
	char *newname = NULL;
	int error;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}
	if (lstat(file->name, &st) != 0) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}
	if (asprintf(&newname, "%s/%s",
	    file->name, req->lr_req.tsymlink.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}
	error = internal_symlink(req->lr_req.tsymlink.symtgt, newname);
	if (error)
		goto out;
	if (lchown(newname, file->uid, req->lr_req.tsymlink.gid) != 0) {
		error = errno;
		goto out;
	}
	if (lstat(newname, &st) != 0) {
		error = errno;
		goto out;
	}

	generate_qid(&st, &req->lr_resp.rsymlink.qid);
out:
	free(newname);
	l9p_respond(req, error);
}

/*
 * Could use a bit more work to reduce code duplication.
 */
static void
fs_mknod(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct stat st;
	uint32_t mode, major, minor;
	char *newname = NULL;
	int error;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}
	if (lstat(file->name, &st) != 0) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}
	if (asprintf(&newname, "%s/%s",
	    file->name, req->lr_req.tmknod.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}

	mode = req->lr_req.tmknod.mode;
	major = req->lr_req.tmknod.major;
	minor = req->lr_req.tmknod.major;

	/*
	 * For now at least, limit to block and character devs only.
	 * Probably need to allow fifos eventually.
	 */
	switch (mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		mode = (mode & S_IFMT) | (mode & 0777);	/* ??? */
		if (mknod(newname, (mode_t)mode, makedev(major, minor)) != 0) {
			error = errno;
			goto out;
		}
		break;
	case S_IFSOCK:
		error = internal_mksocket(file, newname,
		    req->lr_req.tmknod.name);
		if (error != 0)
			goto out;
		break;
	default:
		error = EINVAL;
		goto out;
	}
	if (lchown(newname, file->uid, req->lr_req.tsymlink.gid) != 0) {
		error = errno;
		goto out;
	}
	if (lstat(newname, &st) != 0) {
		error = errno;
		goto out;
	}
	error = 0;

	generate_qid(&st, &req->lr_resp.rmknod.qid);
out:
	free(newname);
	l9p_respond(req, error);
}

static void
fs_rename(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file, *f2;
	struct stat st;
	char *olddirname = NULL, *newname = NULL, *swtmp;
	int error;

	/*
	 * Note: lr_fid represents the file that is to be renamed,
	 * so we must locate its parent directory and verify that
	 * both this parent directory and the new directory f2 are
	 * writable.
	 */
	file = req->lr_fid->lo_aux;
	f2 = req->lr_fid2->lo_aux;
	assert(file && f2);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}
	/* Client probably should not attempt to rename root. */
	if (strcmp(file->name, sc->fs_rootpath) == 0) {
		error = EINVAL;
		goto out;
	}
	olddirname = r_dirname(file->name, NULL, 0);
	if (olddirname == NULL) {
		error = errno;
		goto out;
	}
	if (lstat(olddirname, &st) != 0) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}
	if (strcmp(olddirname, f2->name) != 0) {
		if (lstat(f2->name, &st) != 0) {
			error = errno;
			goto out;
		}
		if (!check_access(&st, f2->uid, L9P_OWRITE)) {
			error = EPERM;
			goto out;
		}
	}
	if (asprintf(&newname, "%s/%s",
	    f2->name, req->lr_req.trename.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}

	if (rename(file->name, newname) != 0) {
		error = errno;
		goto out;
	}
	/* file has been renamed but old fid is not clunked */
	swtmp = newname;
	newname = file->name;
	file->name = swtmp;
	error = 0;

out:
	free(newname);
	free(olddirname);
	l9p_respond(req, error);
}

static void
fs_readlink(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;
	ssize_t linklen;
	char buf[MAXPATHLEN + 1];
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file);

	linklen = readlink(file->name, buf, sizeof(buf));
	if (linklen < 0)
		error = errno;
	else if (linklen > MAXPATHLEN)
		error = ENOMEM; /* todo: allocate dynamically */
	else if ((req->lr_resp.rreadlink.target = strdup(buf)) == NULL)
		error = ENOMEM;
	l9p_respond(req, error);
}

static void
fs_getattr(void *softc __unused, struct l9p_request *req)
{
	uint64_t mask, valid;
	struct openfile *file;
	struct stat st;
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file);

	valid = 0;
	if (lstat(file->name, &st)) {
		error = errno;
		goto out;
	}
	/* ?? Can we provide items not-requested? If so, can skip tests. */
	mask = req->lr_req.tgetattr.request_mask;
	if (mask & L9PL_GETATTR_MODE) {
		/* It is not clear if we need any translations. */
		req->lr_resp.rgetattr.mode = st.st_mode;
		valid |= L9PL_GETATTR_MODE;
	}
	if (mask & L9PL_GETATTR_NLINK) {
		req->lr_resp.rgetattr.nlink = st.st_nlink;
		valid |= L9PL_GETATTR_NLINK;
	}
	if (mask & L9PL_GETATTR_UID) {
		/* provide st_uid, or file->uid? */
		req->lr_resp.rgetattr.uid = st.st_uid;
		valid |= L9PL_GETATTR_UID;
	}
	if (mask & L9PL_GETATTR_GID) {
		/* provide st_gid, or file->gid? */
		req->lr_resp.rgetattr.gid = st.st_gid;
		valid |= L9PL_GETATTR_GID;
	}
	if (mask & L9PL_GETATTR_RDEV) {
		/* It is not clear if we need any translations. */
		req->lr_resp.rgetattr.rdev = (uint64_t)st.st_rdev;
		valid |= L9PL_GETATTR_RDEV;
	}
	if (mask & L9PL_GETATTR_ATIME) {
		req->lr_resp.rgetattr.atime_sec =
		    (uint64_t)st.st_atimespec.tv_sec;
		req->lr_resp.rgetattr.atime_nsec =
		    (uint64_t)st.st_atimespec.tv_nsec;
		valid |= L9PL_GETATTR_ATIME;
	}
	if (mask & L9PL_GETATTR_MTIME) {
		req->lr_resp.rgetattr.mtime_sec =
		    (uint64_t)st.st_mtimespec.tv_sec;
		req->lr_resp.rgetattr.mtime_nsec =
		    (uint64_t)st.st_mtimespec.tv_nsec;
		valid |= L9PL_GETATTR_MTIME;
	}
	if (mask & L9PL_GETATTR_CTIME) {
		req->lr_resp.rgetattr.ctime_sec =
		    (uint64_t)st.st_ctimespec.tv_sec;
		req->lr_resp.rgetattr.ctime_nsec =
		    (uint64_t)st.st_ctimespec.tv_nsec;
		valid |= L9PL_GETATTR_CTIME;
	}
	if (mask & L9PL_GETATTR_BTIME) {
#if defined(HAVE_BIRTHTIME)
		req->lr_resp.rgetattr.btime_sec =
		    (uint64_t)st.st_birthtim.tv_sec;
		req->lr_resp.rgetattr.btime_nsec =
		    (uint64_t)st.st_birthtim.tv_nsec;
#else
		req->lr_resp.rgetattr.btime_sec = 0;
		req->lr_resp.rgetattr.btime_nsec = 0;
#endif
		valid |= L9PL_GETATTR_BTIME;
	}
	if (mask & L9PL_GETATTR_INO)
		valid |= L9PL_GETATTR_INO;
	if (mask & L9PL_GETATTR_SIZE) {
		req->lr_resp.rgetattr.size = (uint64_t)st.st_size;
		valid |= L9PL_GETATTR_SIZE;
	}
	if (mask & L9PL_GETATTR_BLOCKS) {
		req->lr_resp.rgetattr.blksize = (uint64_t)st.st_blksize;
		req->lr_resp.rgetattr.blocks = (uint64_t)st.st_blocks;
		valid |= L9PL_GETATTR_BLOCKS;
	}
	if (mask & L9PL_GETATTR_GEN) {
		req->lr_resp.rgetattr.gen = st.st_gen;
		valid |= L9PL_GETATTR_GEN;
	}
	/* don't know what to do with data version yet */

	generate_qid(&st, &req->lr_resp.rgetattr.qid);
out:
	req->lr_resp.rgetattr.valid = valid;
	l9p_respond(req, error);
}

/*
 * Should combine some of this with wstat code.
 */
static void
fs_setattr(void *softc, struct l9p_request *req)
{
	uint64_t mask;
	struct fs_softc *sc = softc;
	struct timeval tv[2];
	struct openfile *file;
	struct stat st;
	int error = 0;
	uid_t uid, gid;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	/*
	 * As with WSTAT we have atomicity issues.
	 */
	mask = req->lr_req.tsetattr.valid;

	if (lstat(file->name, &st)) {
		error = errno;
		goto out;
	}

	if ((mask & L9PL_SETATTR_SIZE) && S_ISDIR(st.st_mode)) {
		error = EISDIR;
		goto out;
	}

	if (mask & L9PL_SETATTR_MODE) {
		if (lchmod(file->name, req->lr_req.tsetattr.mode & 0777)) {
			error = errno;
			goto out;
		}
	}

	if (mask & (L9PL_SETATTR_UID | L9PL_SETATTR_GID)) {
		uid = mask & L9PL_SETATTR_UID ? req->lr_req.tsetattr.uid :
		    (uid_t)-1;
		gid = mask & L9PL_SETATTR_GID ? req->lr_req.tsetattr.gid :
		    (gid_t)-1;
		if (lchown(file->name, uid, gid)) {
			error = errno;
			goto out;
		}
	}

	if (mask & L9PL_SETATTR_SIZE) {
		/* Truncate follows symlinks, is this OK? */
		if (truncate(file->name, (off_t)req->lr_req.tsetattr.size)) {
			error = errno;
			goto out;
		}
	}

	if (mask & (L9PL_SETATTR_ATIME | L9PL_SETATTR_CTIME)) {
		tv[0].tv_sec = st.st_atimespec.tv_sec;
		tv[0].tv_usec = (int)st.st_atimespec.tv_nsec / 1000;
		tv[1].tv_sec = st.st_mtimespec.tv_sec;
		tv[1].tv_usec = (int)st.st_mtimespec.tv_nsec / 1000;

		if (mask & L9PL_SETATTR_ATIME) {
			if (mask & L9PL_SETATTR_ATIME_SET) {
				tv[0].tv_sec =
				    (long)req->lr_req.tsetattr.atime_sec;
				tv[0].tv_usec =
				    (int)req->lr_req.tsetattr.atime_nsec / 1000;
			} else {
				if (gettimeofday(&tv[0], NULL)) {
					error = errno;
					goto out;
				}
			}
		}
		if (mask & L9PL_SETATTR_MTIME) {
			if (mask & L9PL_SETATTR_MTIME_SET) {
				tv[1].tv_sec =
				    (long)req->lr_req.tsetattr.mtime_sec;
				tv[1].tv_usec =
				    (int)req->lr_req.tsetattr.mtime_nsec / 1000;
			} else {
				if (gettimeofday(&tv[1], NULL)) {
					error = errno;
					goto out;
				}
			}
		}
		if (lutimes(file->name, tv)) {
			error = errno;
			goto out;
		}
	}
out:
	l9p_respond(req, error);
}

static void
fs_xattrwalk(void *softc __unused, struct l9p_request *req)
{
	l9p_respond(req, ENOTSUP);
}

static void
fs_xattrcreate(void *softc __unused, struct l9p_request *req)
{
	l9p_respond(req, ENOTSUP);
}

static void
fs_readdir(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;
	struct l9p_dirent de;
	struct l9p_message msg;
	struct dirent *dp;
	struct stat st;
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (file->dir == NULL) {
		error = ENOTDIR;
		goto out;
	}

	/*
	 * There is no getdirentries variant that accepts an
	 * offset, so once we are multithreaded, this will need
	 * a lock (which will cover the dirent structures as well).
	 *
	 * It's not clear whether we can use the same trick for
	 * discarding offsets here as we do in fs_read.  It
	 * probably should work, we'll have to see if some
	 * client(s) use the zero-offset thing to rescan without
	 * clunking the directory first.
	 */
	if (req->lr_req.io.offset == 0)
		rewinddir(file->dir);
	else
		seekdir(file->dir, (long)req->lr_req.io.offset);

	l9p_init_msg(&msg, req, L9P_PACK);
	while ((dp = readdir(file->dir)) != NULL) {
		/*
		 * Should we skip "." and ".."?  I think so...
		 */
		if (dp->d_name[0] == '.' &&
		    (dp->d_namlen == 1 || strcmp(dp->d_name, "..") == 0))
			continue;

		/*
		 * TODO: we do a full lstat here; could use dp->d_*
		 * to construct the qid more efficiently, as long
		 * as dp->d_type != DT_UNKNOWN.
		 */
		if (fs_lstatat(file, dp->d_name, &st))
			continue;

		de.qid.type = 0;
		generate_qid(&st, &de.qid);
		de.offset = (uint64_t)telldir(file->dir);
		de.type = de.qid.type; /* or dp->d_type? */
		de.name = dp->d_name;

		if (l9p_pudirent(&msg, &de) < 0)
			break;
	}

	req->lr_resp.io.count = (uint32_t)msg.lm_size;
out:
	l9p_respond(req, error);
}

static void
fs_fsync(void *softc __unused, struct l9p_request *req)
{
	struct openfile *file;
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file);
	if (fsync(file->fd))
		error = errno;
	l9p_respond(req, error);
}

static void
fs_lock(void *softc __unused, struct l9p_request *req)
{

	l9p_respond(req, ENOTSUP);
}

static void
fs_getlock(void *softc __unused, struct l9p_request *req)
{

	l9p_respond(req, ENOTSUP);
}

static void
fs_link(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct openfile *dirf;
	struct stat st;
	char *newname;
	int error = 0;

	file = req->lr_fid->lo_aux;
	dirf = req->lr_fid2->lo_aux;
	assert(file && dirf);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	/* Require write access to target directory. */
	if (lstat(dirf->name, &st)) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}

	if (asprintf(&newname, "%s/%s",
	    dirf->name, req->lr_req.tlink.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}
	if (link(file->name, newname) != 0)
		error = errno;
	free(newname);
out:
	l9p_respond(req, error);
}

static void
fs_mkdir(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct stat st;
	char *newname = NULL;
	int error = 0;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	/* Require write access to target directory. */
	if (lstat(file->name, &st)) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}

	if (asprintf(&newname, "%s/%s",
	    file->name, req->lr_req.tmkdir.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}

	error = internal_mkdir(newname, (mode_t)req->lr_req.tmkdir.mode, &st);
	if (error)
		goto out;

	if (lchown(newname, file->uid, req->lr_req.tmkdir.gid) != 0) {
		error = errno;
		goto out;
	}

	if (lstat(newname, &st) != 0) {
		error = errno;
		goto out;
	}

	generate_qid(&st, &req->lr_resp.rmkdir.qid);
out:
	free(newname);
	l9p_respond(req, error);
}

static void
fs_renameat(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *olddir, *newdir;
	struct stat st;
	char *oldname = NULL, *newname = NULL;
	int error;

	olddir = req->lr_fid->lo_aux;
	newdir = req->lr_fid2->lo_aux;
	assert(olddir && newdir);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	/* Require write access to both source and target directory. */
	if (lstat(olddir->name, &st)) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, olddir->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}
	if (olddir != newdir) {
		if (lstat(newdir->name, &st)) {
			error = errno;
			goto out;
		}
		if (!check_access(&st, newdir->uid, L9P_OWRITE)) {
			error = EPERM;
			goto out;
		}
	}

	if (asprintf(&oldname, "%s/%s",
		    olddir->name, req->lr_req.trenameat.oldname) < 0 ||
	    asprintf(&newname, "%s/%s",
		    newdir->name, req->lr_req.trenameat.newname) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}

	error = rename(oldname, newname);

out:
	free(newname);
	free(oldname);
	l9p_respond(req, error);
}

static void
fs_unlinkat(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file;
	struct stat st;
	char *name;
	int error;

	file = req->lr_fid->lo_aux;
	assert(file);

	if (sc->fs_readonly) {
		error = EROFS;
		goto out;
	}

	/* Require write access to directory. */
	if (lstat(file->name, &st)) {
		error = errno;
		goto out;
	}
	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		error = EPERM;
		goto out;
	}

	if (asprintf(&name, "%s/%s",
		    file->name, req->lr_req.tunlinkat.name) < 0) {
		error = ENAMETOOLONG;
		goto out;
	}

	error = unlink(name);
	free(name);
out:
	l9p_respond(req, error);
}

static void
fs_freefid(void *softc __unused, struct l9p_openfile *fid)
{
	struct openfile *f = fid->lo_aux;

	if (f == NULL) {
		/* Nothing to do here */
		return;
	}

	if (f->fd != -1)
		close(f->fd);

	if (f->dir)
		closedir(f->dir);

	free(f->name);
	free(f);
}

int
l9p_backend_fs_init(struct l9p_backend **backendp, const char *root)
{
	struct l9p_backend *backend;
	struct fs_softc *sc;

	backend = l9p_malloc(sizeof(*backend));
	backend->attach = fs_attach;
	backend->clunk = fs_clunk;
	backend->create = fs_create;
	backend->flush = fs_flush;
	backend->open = fs_open;
	backend->read = fs_read;
	backend->remove = fs_remove;
	backend->stat = fs_stat;
	backend->walk = fs_walk;
	backend->write = fs_write;
	backend->wstat = fs_wstat;
	backend->statfs = fs_statfs;
	backend->lopen = fs_lopen;
	backend->lcreate = fs_lcreate;
	backend->symlink = fs_symlink;
	backend->mknod = fs_mknod;
	backend->rename = fs_rename;
	backend->readlink = fs_readlink;
	backend->getattr = fs_getattr;
	backend->setattr = fs_setattr;
	backend->xattrwalk = fs_xattrwalk;
	backend->xattrcreate = fs_xattrcreate;
	backend->readdir = fs_readdir;
	backend->fsync = fs_fsync;
	backend->lock = fs_lock;
	backend->getlock = fs_getlock;
	backend->link = fs_link;
	backend->mkdir = fs_mkdir;
	backend->renameat = fs_renameat;
	backend->unlinkat = fs_unlinkat;
	backend->freefid = fs_freefid;

	sc = l9p_malloc(sizeof(*sc));
	sc->fs_rootpath = strdup(root);
	sc->fs_readonly = false;
	backend->softc = sc;

	setpassent(1);

	*backendp = backend;
	return (0);
}
