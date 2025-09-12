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
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/stat.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/user.h>
#include <sys/vnode.h>

MALLOC_DEFINE(M_JAILDESC, "jaildesc", "jail descriptors");

static fo_poll_t	jaildesc_poll;
static fo_kqfilter_t	jaildesc_kqfilter;
static fo_stat_t	jaildesc_stat;
static fo_close_t	jaildesc_close;
static fo_fill_kinfo_t	jaildesc_fill_kinfo;
static fo_cmp_t		jaildesc_cmp;

static struct fileops jaildesc_ops = {
	.fo_read = invfo_rdwr,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = invfo_ioctl,
	.fo_poll = jaildesc_poll,
	.fo_kqfilter = jaildesc_kqfilter,
	.fo_stat = jaildesc_stat,
	.fo_close = jaildesc_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = jaildesc_fill_kinfo,
	.fo_cmp = jaildesc_cmp,
	.fo_flags = DFLAG_PASSABLE,
};

/*
 * Given a jail descriptor number, return its prison and/or its
 * credential.  They are returned held, and will need to be released
 * by the caller.
 */
int
jaildesc_find(struct thread *td, int fd, struct prison **prp,
    struct ucred **ucredp)
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
	if (prp != NULL) {
		prison_hold(pr);
		*prp = pr;
	}
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

	if (owning) {
		error = priv_check(td, PRIV_JAIL_REMOVE);
		if (error != 0)
			return (error);
	}
	jd = malloc(sizeof(*jd), M_JAILDESC, M_WAITOK | M_ZERO);
	error = falloc_caps(td, &fp, fdp, 0, NULL);
	if (error != 0) {
		free(jd, M_JAILDESC);
		return (error);
	}
	finit(fp, priv_check_cred(fp->f_cred, PRIV_JAIL_SET) == 0 ?
	    FREAD | FWRITE : FREAD, DTYPE_JAILDESC, jd, &jaildesc_ops);
	JAILDESC_LOCK_INIT(jd);
	knlist_init_mtx(&jd->jd_selinfo.si_note, &jd->jd_lock);
	if (owning)
		jd->jd_flags |= JDF_OWNING;
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

/*
 * Pass a note to all listening kqueues.
 */
void
jaildesc_knote(struct prison *pr, long hint)
{
	struct jaildesc *jd;
	int prison_locked;

	if (!LIST_EMPTY(&pr->pr_descs)) {
		prison_locked = mtx_owned(&pr->pr_mtx);
		if (!prison_locked)
			prison_lock(pr);
		LIST_FOREACH(jd, &pr->pr_descs, jd_list) {
			JAILDESC_LOCK(jd);
			if (hint == NOTE_JAIL_REMOVE) {
				jd->jd_flags |= JDF_REMOVED;
				if (jd->jd_flags & JDF_SELECTED) {
					jd->jd_flags &= ~JDF_SELECTED;
					selwakeup(&jd->jd_selinfo);
				}
			}
			KNOTE_LOCKED(&jd->jd_selinfo.si_note, hint);
			JAILDESC_UNLOCK(jd);
		}
		if (!prison_locked)
			prison_unlock(pr);
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
			if (jd->jd_flags & JDF_OWNING) {
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
		knlist_destroy(&jd->jd_selinfo.si_note);
		JAILDESC_LOCK_DESTROY(jd);
		free(jd, M_JAILDESC);
	}
	return (0);
}

static int
jaildesc_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct jaildesc *jd;
	int revents;

	revents = 0;
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	if (jd->jd_flags & JDF_REMOVED)
		revents |= POLLHUP;
	if (revents == 0) {
		selrecord(td, &jd->jd_selinfo);
		jd->jd_flags |= JDF_SELECTED;
	}
	JAILDESC_UNLOCK(jd);
	return (revents);
}

static void
jaildesc_kqops_detach(struct knote *kn)
{
	struct jaildesc *jd;

	jd = kn->kn_fp->f_data;
	knlist_remove(&jd->jd_selinfo.si_note, kn, 0);
}

static int
jaildesc_kqops_event(struct knote *kn, long hint)
{
	struct jaildesc *jd;
	u_int event;

	jd = kn->kn_fp->f_data;
	if (hint == 0) {
		/*
		 * Initial test after registration. Generate a
		 * NOTE_JAIL_REMOVE in case the prison already died
		 * before registration.
		 */
		event = jd->jd_flags & JDF_REMOVED ? NOTE_JAIL_REMOVE : 0;
	} else {
		/*
		 * Mask off extra data.  In the NOTE_JAIL_CHILD case,
		 * that's everything except the NOTE_JAIL_CHILD bit
		 * itself, since a JID is any positive integer.
		 */
		event = ((u_int)hint & NOTE_JAIL_CHILD) ? NOTE_JAIL_CHILD :
		    (u_int)hint & NOTE_JAIL_CTRLMASK;
	}

	/* If the user is interested in this event, record it. */
	if (kn->kn_sfflags & event) {
		kn->kn_fflags |= event;
		/* Report the created jail id or attached process id. */
		if (event == NOTE_JAIL_CHILD || event == NOTE_JAIL_ATTACH) {
			if (kn->kn_data != 0)
				kn->kn_fflags |= NOTE_JAIL_MULTI;
			kn->kn_data = (kn->kn_fflags & NOTE_JAIL_MULTI) ? 0U :
			    (u_int)hint & ~event;
		}
	}

	/* Prison is gone, so flag the event as finished. */
	if (event == NOTE_JAIL_REMOVE) {
		kn->kn_flags |= EV_EOF | EV_ONESHOT;
		if (kn->kn_fflags == 0)
			kn->kn_flags |= EV_DROP;
		return (1);
	}

	return (kn->kn_fflags != 0);
}

static const struct filterops jaildesc_kqops = {
	.f_isfd = 1,
	.f_detach = jaildesc_kqops_detach,
	.f_event = jaildesc_kqops_event,
};

static int
jaildesc_kqfilter(struct file *fp, struct knote *kn)
{
	struct jaildesc *jd;

	jd = fp->f_data;
	switch (kn->kn_filter) {
	case EVFILT_JAILDESC:
		kn->kn_fop = &jaildesc_kqops;
		kn->kn_flags |= EV_CLEAR;
		knlist_add(&jd->jd_selinfo.si_note, kn, 0);
		return (0);
	default:
		return (EINVAL);
	}
}

static int
jaildesc_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{
	struct jaildesc *jd;

	bzero(sb, sizeof(struct stat));
	jd = fp->f_data;
	JAILDESC_LOCK(jd);
	if (jd->jd_prison != NULL) {
		sb->st_ino = jd->jd_prison->pr_id;
		sb->st_mode = S_IFREG | S_IRWXU;
	} else
		sb->st_mode = S_IFREG;
	JAILDESC_UNLOCK(jd);
	return (0);
}

static int
jaildesc_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	struct jaildesc *jd;

	jd = fp->f_data;
	kif->kf_type = KF_TYPE_JAILDESC;
	kif->kf_un.kf_jail.kf_jid = jd->jd_prison ? jd->jd_prison->pr_id : 0;
	return (0);
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
