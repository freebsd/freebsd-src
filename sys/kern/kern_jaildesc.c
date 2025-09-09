/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 James Gritton.
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
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/jaildesc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/stat.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

MALLOC_DEFINE(M_JAILDESC, "jaildesc", "jail descriptors");

static fo_stat_t	jaildesc_stat;
static fo_close_t	jaildesc_close;
static fo_chmod_t	jaildesc_chmod;
static fo_chown_t	jaildesc_chown;
static fo_fill_kinfo_t	jaildesc_fill_kinfo;
static fo_cmp_t		jaildesc_cmp;

static struct fileops jaildesc_ops = {
	.fo_read = invfo_rdwr,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = invfo_ioctl,
	.fo_poll = invfo_poll,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = jaildesc_stat,
	.fo_close = jaildesc_close,
	.fo_chmod = jaildesc_chmod,
	.fo_chown = jaildesc_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = jaildesc_fill_kinfo,
	.fo_cmp = jaildesc_cmp,
	.fo_flags = DFLAG_PASSABLE,
};

/*
 * Given a jail descriptor number, return the jaildesc, its prison,
 * and its credential.  The jaildesc will be returned locked, and
 * prison and the credential will be returned held.
 */
int
jaildesc_find(struct thread *td, int fd, struct jaildesc **jdp,
    struct prison **prp, struct ucred **ucredp)
{
	struct file *fp;
	struct jaildesc *jd;
	struct prison *pr;
	int error;

	error = fget(td, fd, &cap_no_rights, &fp);
	if (error != 0)
		return (error);
	if (fp->f_type != DTYPE_JAILDESC) {
		error = EINVAL;
		goto out;
	}
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	pr = jd->jd_prison;
	if (pr == NULL || !prison_isvalid(pr)) {
		error = ENOENT;
		JAILDESC_UNLOCK(jd);
		goto out;
	}
	prison_hold(pr);
	*prp = pr;
	if (jdp != NULL)
		*jdp = jd;
	else
		JAILDESC_UNLOCK(jd);
	if (ucredp != NULL)
		*ucredp = crhold(fp->f_cred);
 out:
	fdrop(fp, td);
	return (error);
}

/*
 * Allocate a new jail decriptor, not yet associated with a prison.
 * Return the file pointer (with a reference held) and the descriptor
 * number.
 */
int
jaildesc_alloc(struct thread *td, struct file **fpp, int *fdp, int owning)
{
	struct file *fp;
	struct jaildesc *jd;
	int error;
	mode_t mode;

	if (owning) {
		error = priv_check(td, PRIV_JAIL_REMOVE);
		if (error != 0)
			return (error);
		mode = S_ISTXT;
	} else
		mode = 0;
	jd = malloc(sizeof(*jd), M_JAILDESC, M_WAITOK | M_ZERO);
	error = falloc_caps(td, &fp, fdp, 0, NULL);
	if (error != 0) {
		free(jd, M_JAILDESC);
		return (error);
	}
	finit(fp, priv_check_cred(fp->f_cred, PRIV_JAIL_SET) == 0 ?
	    FREAD | FWRITE : FREAD, DTYPE_JAILDESC, jd, &jaildesc_ops);
	JAILDESC_LOCK_INIT(jd);
	jd->jd_uid = fp->f_cred->cr_uid;
	jd->jd_gid = fp->f_cred->cr_gid;
	jd->jd_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | mode |
	    (priv_check(td, PRIV_JAIL_SET) == 0 ? S_IWUSR | S_IXUSR : 0) |
	    (priv_check(td, PRIV_JAIL_ATTACH) == 0 ? S_IXUSR : 0);
	*fpp = fp;
	return (0);
}

/*
 * Assocate a jail descriptor with its prison.
 */
void
jaildesc_set_prison(struct file *fp, struct prison *pr)
{
	struct jaildesc *jd;

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	jd->jd_prison = pr;
	LIST_INSERT_HEAD(&pr->pr_descs, jd, jd_list);
	prison_hold(pr);
	JAILDESC_UNLOCK(jd);
}

/*
 * Detach all the jail descriptors from a prison.
 */
void
jaildesc_prison_cleanup(struct prison *pr)
{
	struct jaildesc *jd;

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	while ((jd = LIST_FIRST(&pr->pr_descs))) {
		JAILDESC_LOCK(jd);
		LIST_REMOVE(jd, jd_list);
		jd->jd_prison = NULL;
		JAILDESC_UNLOCK(jd);
		prison_free(pr);
	}
}

static int
jaildesc_close(struct file *fp, struct thread *td)
{
	struct jaildesc *jd;
	struct prison *pr;

	jd = fp->f_data;
	fp->f_data = NULL;
	if (jd != NULL) {
		JAILDESC_LOCK(jd);
		pr = jd->jd_prison;
		if (pr != NULL) {
			/*
			 * Free or remove the associated prison.
			 * This requires a second check after re-
			 * ordering locks.  This jaildesc can remain
			 * unlocked once we have a prison reference,
			 * because that prison is the only place that
			 * still points back to it.
			 */
			prison_hold(pr);
			JAILDESC_UNLOCK(jd);
			if (jd->jd_mode & S_ISTXT) {
				sx_xlock(&allprison_lock);
				prison_lock(pr);
				if (jd->jd_prison != NULL) {
					/*
					 * Unlink the prison, but don't free
					 * it; that will be done as part of
					 * of prison_remove.
					 */
					LIST_REMOVE(jd, jd_list);
					prison_remove(pr);
				} else {
					prison_unlock(pr);
					sx_xunlock(&allprison_lock);
				}
			} else {
				prison_lock(pr);
				if (jd->jd_prison != NULL) {
					LIST_REMOVE(jd, jd_list);
					prison_free(pr);
				}
				prison_unlock(pr);
			}
			prison_free(pr);
		}
		JAILDESC_LOCK_DESTROY(jd);
		free(jd, M_JAILDESC);
	}
	return (0);
}

static int
jaildesc_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{
	struct jaildesc *jd;

	bzero(sb, sizeof(struct stat));
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	if (jd->jd_prison != NULL) {
		sb->st_ino = jd->jd_prison ? jd->jd_prison->pr_id : 0;
		sb->st_uid = jd->jd_uid;
		sb->st_gid = jd->jd_gid;
		sb->st_mode = jd->jd_mode;
	} else
		sb->st_mode = S_IFREG;
	JAILDESC_UNLOCK(jd);
	return (0);
}

static int
jaildesc_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{
	struct jaildesc *jd;
	int error;

	/* Reject permissions that the creator doesn't have. */
	if (((mode & (S_IWUSR | S_IWGRP | S_IWOTH)) &&
	    priv_check_cred(fp->f_cred, PRIV_JAIL_SET) != 0) ||
	    ((mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
	    priv_check_cred(fp->f_cred, PRIV_JAIL_ATTACH) != 0 &&
	    priv_check_cred(fp->f_cred, PRIV_JAIL_SET) != 0) ||
	    ((mode & S_ISTXT) &&
	    priv_check_cred(fp->f_cred, PRIV_JAIL_REMOVE) != 0))
		return (EPERM);
	if (mode & (S_ISUID | S_ISGID))
		return (EINVAL);
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	error = vaccess(VREG, jd->jd_mode, jd->jd_uid, jd->jd_gid, VADMIN,
		active_cred);
	if (error == 0)
		jd->jd_mode = S_IFREG | (mode & ALLPERMS);
	JAILDESC_UNLOCK(jd);
	return (error);
}

static int
jaildesc_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{
	struct jaildesc *jd;
	int error;

	error = 0;
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	if (uid == (uid_t)-1)
		uid = jd->jd_uid;
	if (gid == (gid_t)-1)
		gid = jd->jd_gid;
	if ((uid != jd->jd_uid && uid != active_cred->cr_uid) ||
	    (gid != jd->jd_gid && !groupmember(gid, active_cred)))
		error = priv_check_cred(active_cred, PRIV_VFS_CHOWN);
	if (error == 0) {
		jd->jd_uid = uid;
		jd->jd_gid = gid;
	}
	JAILDESC_UNLOCK(jd);
	return (error);
}

static int
jaildesc_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	return (EINVAL);
}

static int
jaildesc_cmp(struct file *fp1, struct file *fp2, struct thread *td)
{
	struct jaildesc *jd1, *jd2;
	int jid1, jid2;

	if (fp2->f_type != DTYPE_JAILDESC)
		return (3);
	jd1 = fp1->f_data;
	JAILDESC_LOCK(jd1);
	jid1 = jd1->jd_prison ? (uintptr_t)jd1->jd_prison->pr_id : 0;
	JAILDESC_UNLOCK(jd1);
	jd2 = fp2->f_data;
	JAILDESC_LOCK(jd2);
	jid2 = jd2->jd_prison ? (uintptr_t)jd2->jd_prison->pr_id : 0;
	JAILDESC_UNLOCK(jd2);
	return (kcmp_cmp(jid1, jid2));
}
