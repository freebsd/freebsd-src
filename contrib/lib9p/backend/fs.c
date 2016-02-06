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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <sys/socket.h>
#include "../lib9p.h"
#include "../lib9p_impl.h"
#include "../log.h"

static struct openfile *open_fid(const char *);
static void dostat(struct l9p_stat *, char *, struct stat *, bool dotu);
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
static void fs_freefid(void *softc, struct l9p_openfile *f);

struct fs_softc
{
	const char *fs_rootpath;
	bool fs_readonly;
	TAILQ_HEAD(, fs_tree) fs_auxtrees;
};

struct fs_tree
{
	const char *fst_name;
	const char *fst_path;
	bool fst_readonly;
	TAILQ_ENTRY(fs_tree) fst_link;
};

struct openfile
{
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

	/* XXX: not thread safe */
	s->name = strdup(basename(name));

	if (!dotu) {
		user = getpwuid(buf->st_uid);
		group = getgrgid(buf->st_gid);
		s->uid = user != NULL ? strdup(user->pw_name) : NULL;
		s->gid = group != NULL ? strdup(group->gr_name) : NULL;
		s->muid = user != NULL ? strdup(user->pw_name) : NULL;
	} else {
		/*
		 * When using 9P2000.u, we don't need to bother about
		 * providing user and group names in textual form.
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
	int groups[NGROUPS_MAX];
	int ngroups = NGROUPS_MAX;
	int i;

	if (uid == 0)
		return (true);

	if (st->st_uid == uid) {
		if (amode == L9P_OREAD && st->st_mode & S_IRUSR)
			return (true);
		
		if (amode == L9P_OWRITE && st->st_mode & S_IWUSR)
			return (true);
		
		if (amode == L9P_OEXEC && st->st_mode & S_IXUSR)
			return (true);
	}

	/* Check for "other" access */
	if (amode == L9P_OREAD && st->st_mode & S_IROTH)
		return (true);

	if (amode == L9P_OWRITE && st->st_mode & S_IWOTH)
		return (true);

	if (amode == L9P_OEXEC && st->st_mode & S_IXOTH)
		return (true);

	/* Check for group access */
	pwd = getpwuid(uid);
	getgrouplist(pwd->pw_name, (int)pwd->pw_gid, groups, &ngroups);

	for (i = 0; i < ngroups; i++) {
		if (st->st_gid == (gid_t)groups[i]) {
			if (amode == L9P_OREAD && st->st_mode & S_IRGRP)
				return (true);

			if (amode == L9P_OWRITE && st->st_mode & S_IWGRP)
				return (true);

			if (amode == L9P_OEXEC && st->st_mode & S_IXGRP)
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

static void
fs_create(void *softc, struct l9p_request *req)
{
	struct fs_softc *sc = softc;
	struct openfile *file = req->lr_fid->lo_aux;
	struct stat st;
	char *newname;
	mode_t mode = req->lr_req.tcreate.perm & 0777;

	assert(file != NULL);

	if (sc->fs_readonly) {
		l9p_respond(req, EROFS);
		return;
	}

	asprintf(&newname, "%s/%s", file->name, req->lr_req.tcreate.name);

	if (stat(file->name, &st) != 0) {
		l9p_respond(req, errno);
		return;
	}

	if (!check_access(&st, file->uid, L9P_OWRITE)) {
		l9p_respond(req, EPERM);
		return;
	}

	if (req->lr_req.tcreate.perm & L9P_DMDIR)
		mkdir(newname, mode);
	else if (req->lr_req.tcreate.perm & L9P_DMSYMLINK) {
		if (symlink(req->lr_req.tcreate.extension, newname) != 0) {
			l9p_respond(req, errno);
			return;
		}
	} else if (req->lr_req.tcreate.perm & L9P_DMNAMEDPIPE) {
		if (mkfifo(newname, mode) != 0) {
			l9p_respond(req, errno);
			return;
		}
	} else if (req->lr_req.tcreate.perm & L9P_DMSOCKET) {
		struct sockaddr_un sun;
		int s = socket(AF_UNIX, SOCK_STREAM, 0);

		if (s < 0) {
			l9p_respond(req, errno);
			return;
		}

		sun.sun_family = AF_UNIX;
		sun.sun_len = sizeof(struct sockaddr_un);
		strncpy(sun.sun_path, newname, sizeof(sun.sun_path));

		if (bind(s, (struct sockaddr *)&sun, sun.sun_len) < 0) {
			l9p_respond(req, errno);
			return;
		}

		if (close(s) != 0) {
			l9p_respond(req, errno);
			return;
		}
	} else if (req->lr_req.tcreate.perm & L9P_DMDEVICE) {
		char type;
		int major, minor;

		if (sscanf(req->lr_req.tcreate.extension, "%c %u %u",
		    &type, &major, &minor) < 2) {
			l9p_respond(req, EINVAL);
			return;
		}

		switch (type) {
		case 'b':
			if (mknod(newname, S_IFBLK | mode,
			    makedev(major, minor)) != 0)
			{
				l9p_respond(req, errno);
				return;
			}
			break;
		case 'c':
			if (mknod(newname, S_IFCHR | mode,
			    makedev(major, minor)) != 0)
			{
				l9p_respond(req, errno);
				return;
			}
			break;
		default:
			l9p_respond(req, EINVAL);
			return;
		}
	} else {
		file->fd = open(newname,
		    O_CREAT | O_TRUNC | req->lr_req.tcreate.mode,
		    mode);
	}

	if (lchown(newname, file->uid, file->gid) != 0) {
		l9p_respond(req, errno);
		return;
	}

	if (stat(newname, &st) != 0) {
		l9p_respond(req, errno);
		return;
	}

	generate_qid(&st, &req->lr_resp.rcreate.qid);
	l9p_respond(req, 0);
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

	if (S_ISDIR(st.st_mode))
		file->dir = opendir(file->name);
	else {
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

		for (;;) {
			d = readdir(file->dir);
			if (d) {
				lstat(d->d_name, &st);
				dostat(&l9stat, d->d_name, &st, dotu);
				if (l9p_pack_stat(req, &l9stat) != 0) {
					seekdir(file->dir, -1);
					break;
				}

				continue;
			}

			break;
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
fs_walk(void *softc __unused, struct l9p_request *req)
{
	uint16_t i;
	struct stat buf;
	struct openfile *file = req->lr_fid->lo_aux;
	struct openfile *newfile;
	char name[MAXPATHLEN];

	strcpy(name, file->name);

	for (i = 0; i < req->lr_req.twalk.nwname; i++) {
		strcat(name, "/");
		strcat(name, req->lr_req.twalk.wname[i]);
		if (lstat(name, &buf) < 0){
			l9p_respond(req, ENOENT);
			return;
		}

		generate_qid(&buf, &req->lr_resp.rwalk.wqid[i]);
	}

	newfile = open_fid(name);
	newfile->uid = file->uid;
	newfile->gid = file->gid;
	req->lr_newfid->lo_aux = newfile;
	req->lr_resp.rwalk.nwqid = i;
	l9p_respond(req, 0);
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
		l9p_respond(req, EROFS);
		return;
	}

	if (l9stat->atime != (uint32_t)~0) {
		/* XXX: not implemented, ignore */
	}

	if (l9stat->mtime != (uint32_t)~0) {
		/* XXX: not implemented, ignore */
	}

	if (l9stat->dev != (uint32_t)~0) {
		l9p_respond(req, EPERM);
		return;
	}

	if (l9stat->length != (uint64_t)~0) {
		if (file->dir != NULL) {
			l9p_respond(req, EINVAL);
			return;
		}
		
		if (truncate(file->name, (off_t)l9stat->length) != 0) {
			l9p_respond(req, errno);
			return;
		}
	}

	if (req->lr_conn->lc_version >= L9P_2000U) {
		if (lchown(file->name, l9stat->n_uid, l9stat->n_gid) != 0) {
			l9p_respond(req, errno);
			return;
		}
	}

	if (l9stat->mode != (uint32_t)~0) {
		if (chmod(file->name, l9stat->mode & 0777) != 0) {
			l9p_respond(req, errno);
			return;
		}
	}

	if (strlen(l9stat->name) > 0) {
		/* XXX: not thread safe */
		char *dir = dirname(file->name);
		char *newname;
		
		asprintf(&newname, "%s/%s", dir, l9stat->name);
		rename(file->name, newname);
		
		free(newname);
	}

	l9p_respond(req, 0);
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
	backend->freefid = fs_freefid;

	sc = l9p_malloc(sizeof(*sc));
	sc->fs_rootpath = strdup(root);
	sc->fs_readonly = false;
	backend->softc = sc;

	setpassent(1);
	
	*backendp = backend;
	return (0);
}
