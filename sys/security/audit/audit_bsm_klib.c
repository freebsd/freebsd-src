/*
 * Copyright (c) 1999-2009 Apple Inc.
 * Copyright (c) 2005, 2016 Robert N. M. Watson
 * All rights reserved.
 *
 * Portions of this software were developed by BAE Systems, the University of
 * Cambridge Computer Laboratory, and Memorial University under DARPA/AFRL
 * contract FA8650-15-C-7558 ("CADETS"), as part of the DARPA Transparent
 * Computing (TC) research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sem.h>
#include <sys/sbuf.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>

#include <bsm/audit.h>
#include <bsm/audit_kevents.h>
#include <security/audit/audit.h>
#include <security/audit/audit_private.h>

/*
 * Hash table functions for the audit event number to event class mask
 * mapping.
 */
#define	EVCLASSMAP_HASH_TABLE_SIZE	251
struct evclass_elem {
	au_event_t event;
	au_class_t class;
	LIST_ENTRY(evclass_elem) entry;
};
struct evclass_list {
	LIST_HEAD(, evclass_elem) head;
};

static MALLOC_DEFINE(M_AUDITEVCLASS, "audit_evclass", "Audit event class");
static struct rwlock		evclass_lock;
static struct evclass_list	evclass_hash[EVCLASSMAP_HASH_TABLE_SIZE];

#define	EVCLASS_LOCK_INIT()	rw_init(&evclass_lock, "evclass_lock")
#define	EVCLASS_RLOCK()		rw_rlock(&evclass_lock)
#define	EVCLASS_RUNLOCK()	rw_runlock(&evclass_lock)
#define	EVCLASS_WLOCK()		rw_wlock(&evclass_lock)
#define	EVCLASS_WUNLOCK()	rw_wunlock(&evclass_lock)

/*
 * Hash table maintaining a mapping from audit event numbers to audit event
 * names.  For now, used only by DTrace, but present always so that userspace
 * tools can register and inspect fields consistently even if DTrace is not
 * present.
 *
 * struct evname_elem is defined in audit_private.h so that audit_dtrace.c can
 * use the definition.
 */
#define	EVNAMEMAP_HASH_TABLE_SIZE	251
struct evname_list {
	LIST_HEAD(, evname_elem)	enl_head;
};

static MALLOC_DEFINE(M_AUDITEVNAME, "audit_evname", "Audit event name");
static struct sx		evnamemap_lock;
static struct evname_list	evnamemap_hash[EVNAMEMAP_HASH_TABLE_SIZE];

#define	EVNAMEMAP_LOCK_INIT()	sx_init(&evnamemap_lock, "evnamemap_lock");
#define	EVNAMEMAP_RLOCK()	sx_slock(&evnamemap_lock)
#define	EVNAMEMAP_RUNLOCK()	sx_sunlock(&evnamemap_lock)
#define	EVNAMEMAP_WLOCK()	sx_xlock(&evnamemap_lock)
#define	EVNAMEMAP_WUNLOCK()	sx_xunlock(&evnamemap_lock)

struct aue_open_event {
	int		aoe_flags;
	au_event_t	aoe_event;
};

static const struct aue_open_event aue_open[] = {
	{ O_RDONLY,					AUE_OPEN_R },
	{ (O_RDONLY | O_CREAT),				AUE_OPEN_RC },
	{ (O_RDONLY | O_CREAT | O_TRUNC),		AUE_OPEN_RTC },
	{ (O_RDONLY | O_TRUNC),				AUE_OPEN_RT },
	{ O_RDWR,					AUE_OPEN_RW },
	{ (O_RDWR | O_CREAT),				AUE_OPEN_RWC },
	{ (O_RDWR | O_CREAT | O_TRUNC),			AUE_OPEN_RWTC },
	{ (O_RDWR | O_TRUNC),				AUE_OPEN_RWT },
	{ O_WRONLY,					AUE_OPEN_W },
	{ (O_WRONLY | O_CREAT),				AUE_OPEN_WC },
	{ (O_WRONLY | O_CREAT | O_TRUNC),		AUE_OPEN_WTC },
	{ (O_WRONLY | O_TRUNC),				AUE_OPEN_WT },
};

static const struct aue_open_event aue_openat[] = {
	{ O_RDONLY,					AUE_OPENAT_R },
	{ (O_RDONLY | O_CREAT),				AUE_OPENAT_RC },
	{ (O_RDONLY | O_CREAT | O_TRUNC),		AUE_OPENAT_RTC },
	{ (O_RDONLY | O_TRUNC),				AUE_OPENAT_RT },
	{ O_RDWR,					AUE_OPENAT_RW },
	{ (O_RDWR | O_CREAT),				AUE_OPENAT_RWC },
	{ (O_RDWR | O_CREAT | O_TRUNC),			AUE_OPENAT_RWTC },
	{ (O_RDWR | O_TRUNC),				AUE_OPENAT_RWT },
	{ O_WRONLY,					AUE_OPENAT_W },
	{ (O_WRONLY | O_CREAT),				AUE_OPENAT_WC },
	{ (O_WRONLY | O_CREAT | O_TRUNC),		AUE_OPENAT_WTC },
	{ (O_WRONLY | O_TRUNC),				AUE_OPENAT_WT },
};

/*
 * Look up the class for an audit event in the class mapping table.
 */
au_class_t
au_event_class(au_event_t event)
{
	struct evclass_list *evcl;
	struct evclass_elem *evc;
	au_class_t class;

	EVCLASS_RLOCK();
	evcl = &evclass_hash[event % EVCLASSMAP_HASH_TABLE_SIZE];
	class = 0;
	LIST_FOREACH(evc, &evcl->head, entry) {
		if (evc->event == event) {
			class = evc->class;
			goto out;
		}
	}
out:
	EVCLASS_RUNLOCK();
	return (class);
}

/*
 * Insert a event to class mapping. If the event already exists in the
 * mapping, then replace the mapping with the new one.
 *
 * XXX There is currently no constraints placed on the number of mappings.
 * May want to either limit to a number, or in terms of memory usage.
 */
void
au_evclassmap_insert(au_event_t event, au_class_t class)
{
	struct evclass_list *evcl;
	struct evclass_elem *evc, *evc_new;

	/*
	 * Pessimistically, always allocate storage before acquiring mutex.
	 * Free if there is already a mapping for this event.
	 */
	evc_new = malloc(sizeof(*evc), M_AUDITEVCLASS, M_WAITOK);

	EVCLASS_WLOCK();
	evcl = &evclass_hash[event % EVCLASSMAP_HASH_TABLE_SIZE];
	LIST_FOREACH(evc, &evcl->head, entry) {
		if (evc->event == event) {
			evc->class = class;
			EVCLASS_WUNLOCK();
			free(evc_new, M_AUDITEVCLASS);
			return;
		}
	}
	evc = evc_new;
	evc->event = event;
	evc->class = class;
	LIST_INSERT_HEAD(&evcl->head, evc, entry);
	EVCLASS_WUNLOCK();
}

void
au_evclassmap_init(void)
{
	int i;

	EVCLASS_LOCK_INIT();
	for (i = 0; i < EVCLASSMAP_HASH_TABLE_SIZE; i++)
		LIST_INIT(&evclass_hash[i].head);

	/*
	 * Set up the initial event to class mapping for system calls.
	 *
	 * XXXRW: Really, this should walk all possible audit events, not all
	 * native ABI system calls, as there may be audit events reachable
	 * only through non-native system calls.  It also seems a shame to
	 * frob the mutex this early.
	 */
	for (i = 0; i < SYS_MAXSYSCALL; i++) {
		if (sysent[i].sy_auevent != AUE_NULL)
			au_evclassmap_insert(sysent[i].sy_auevent, 0);
	}
}

/*
 * Check whether an event is aditable by comparing the mask of classes this
 * event is part of against the given mask.
 */
int
au_preselect(au_event_t event, au_class_t class, au_mask_t *mask_p, int sorf)
{
	au_class_t effmask = 0;

	if (mask_p == NULL)
		return (-1);

	/*
	 * Perform the actual check of the masks against the event.
	 */
	if (sorf & AU_PRS_SUCCESS)
		effmask |= (mask_p->am_success & class);

	if (sorf & AU_PRS_FAILURE)
		effmask |= (mask_p->am_failure & class);

	if (effmask)
		return (1);
	else
		return (0);
}

/*
 * Look up the name for an audit event in the event-to-name mapping table.
 */
int
au_event_name(au_event_t event, char *name)
{
	struct evname_list *enl;
	struct evname_elem *ene;
	int error;

	error = ENOENT;
	EVNAMEMAP_RLOCK();
	enl = &evnamemap_hash[event % EVNAMEMAP_HASH_TABLE_SIZE];
	LIST_FOREACH(ene, &enl->enl_head, ene_entry) {
		if (ene->ene_event == event) {
			strlcpy(name, ene->ene_name, EVNAMEMAP_NAME_SIZE);
			error = 0;
			goto out;
		}
	}
out:
	EVNAMEMAP_RUNLOCK();
	return (error);
}

/*
 * Insert a event-to-name mapping.  If the event already exists in the
 * mapping, then replace the mapping with the new one.
 *
 * XXX There is currently no constraints placed on the number of mappings.
 * May want to either limit to a number, or in terms of memory usage.
 *
 * XXXRW: Accepts truncated name -- but perhaps should return failure instead?
 *
 * XXXRW: It could be we need a way to remove existing names...?
 *
 * XXXRW: We handle collisions between numbers, but I wonder if we also need a
 * way to handle name collisions, for DTrace, where probe names must be
 * unique?
 */
void
au_evnamemap_insert(au_event_t event, const char *name)
{
	struct evname_list *enl;
	struct evname_elem *ene, *ene_new;

	/*
	 * Pessimistically, always allocate storage before acquiring lock.
	 * Free if there is already a mapping for this event.
	 */
	ene_new = malloc(sizeof(*ene_new), M_AUDITEVNAME, M_WAITOK | M_ZERO);
	EVNAMEMAP_WLOCK();
	enl = &evnamemap_hash[event % EVNAMEMAP_HASH_TABLE_SIZE];
	LIST_FOREACH(ene, &enl->enl_head, ene_entry) {
		if (ene->ene_event == event) {
			EVNAME_LOCK(ene);
			(void)strlcpy(ene->ene_name, name,
			    sizeof(ene->ene_name));
			EVNAME_UNLOCK(ene);
			EVNAMEMAP_WUNLOCK();
			free(ene_new, M_AUDITEVNAME);
			return;
		}
	}
	ene = ene_new;
	mtx_init(&ene->ene_lock, "au_evnamemap", NULL, MTX_DEF);
	ene->ene_event = event;
	(void)strlcpy(ene->ene_name, name, sizeof(ene->ene_name));
	LIST_INSERT_HEAD(&enl->enl_head, ene, ene_entry);
	EVNAMEMAP_WUNLOCK();
}

void
au_evnamemap_init(void)
{
	int i;

	EVNAMEMAP_LOCK_INIT();
	for (i = 0; i < EVNAMEMAP_HASH_TABLE_SIZE; i++)
		LIST_INIT(&evnamemap_hash[i].enl_head);

	/*
	 * XXXRW: Unlike the event-to-class mapping, we don't attempt to
	 * pre-populate the list.  Perhaps we should...?  But not sure we
	 * really want to duplicate /etc/security/audit_event in the kernel
	 * -- and we'd need a way to remove names?
	 */
}

/*
 * The DTrace audit provider occasionally needs to walk the entries in the
 * event-to-name mapping table, and uses this public interface to do so.  A
 * write lock is acquired so that the provider can safely update its fields in
 * table entries.
 */
void
au_evnamemap_foreach(au_evnamemap_callback_t callback)
{
	struct evname_list *enl;
	struct evname_elem *ene;
	int i;

	EVNAMEMAP_WLOCK();
	for (i = 0; i < EVNAMEMAP_HASH_TABLE_SIZE; i++) {
		enl = &evnamemap_hash[i];
		LIST_FOREACH(ene, &enl->enl_head, ene_entry)
			callback(ene);
	}
	EVNAMEMAP_WUNLOCK();
}

/*
 * Convert sysctl names and present arguments to events.
 */
au_event_t
audit_ctlname_to_sysctlevent(int name[], uint64_t valid_arg)
{

	/* can't parse it - so return the worst case */
	if ((valid_arg & (ARG_CTLNAME | ARG_LEN)) != (ARG_CTLNAME | ARG_LEN))
		return (AUE_SYSCTL);

	switch (name[0]) {
	/* non-admin "lookups" treat them special */
	case KERN_OSTYPE:
	case KERN_OSRELEASE:
	case KERN_OSREV:
	case KERN_VERSION:
	case KERN_ARGMAX:
	case KERN_CLOCKRATE:
	case KERN_BOOTTIME:
	case KERN_POSIX1:
	case KERN_NGROUPS:
	case KERN_JOB_CONTROL:
	case KERN_SAVED_IDS:
	case KERN_OSRELDATE:
	case KERN_DUMMY:
		return (AUE_SYSCTL_NONADMIN);

	/* only treat the changeable controls as admin */
	case KERN_MAXVNODES:
	case KERN_MAXPROC:
	case KERN_MAXFILES:
	case KERN_MAXPROCPERUID:
	case KERN_MAXFILESPERPROC:
	case KERN_HOSTID:
	case KERN_SECURELVL:
	case KERN_HOSTNAME:
	case KERN_VNODE:
	case KERN_PROC:
	case KERN_FILE:
	case KERN_PROF:
	case KERN_NISDOMAINNAME:
	case KERN_UPDATEINTERVAL:
	case KERN_NTP_PLL:
	case KERN_BOOTFILE:
	case KERN_DUMPDEV:
	case KERN_IPC:
	case KERN_PS_STRINGS:
	case KERN_USRSTACK:
	case KERN_LOGSIGEXIT:
	case KERN_IOV_MAX:
		return ((valid_arg & ARG_VALUE) ?
		    AUE_SYSCTL : AUE_SYSCTL_NONADMIN);

	default:
		return (AUE_SYSCTL);
	}
	/* NOTREACHED */
}

/*
 * Convert an open flags specifier into a specific type of open event for
 * auditing purposes.
 */
au_event_t
audit_flags_and_error_to_openevent(int oflags, int error)
{
	int i;

	/*
	 * Need to check only those flags we care about.
	 */
	oflags = oflags & (O_RDONLY | O_CREAT | O_TRUNC | O_RDWR | O_WRONLY);
	for (i = 0; i < nitems(aue_open); i++) {
		if (aue_open[i].aoe_flags == oflags)
			return (aue_open[i].aoe_event);
	}
	return (AUE_OPEN);
}

au_event_t
audit_flags_and_error_to_openatevent(int oflags, int error)
{
	int i;

	/*
	 * Need to check only those flags we care about.
	 */
	oflags = oflags & (O_RDONLY | O_CREAT | O_TRUNC | O_RDWR | O_WRONLY);
	for (i = 0; i < nitems(aue_openat); i++) {
		if (aue_openat[i].aoe_flags == oflags)
			return (aue_openat[i].aoe_event);
	}
	return (AUE_OPENAT);
}

/*
 * Convert a MSGCTL command to a specific event.
 */
au_event_t
audit_msgctl_to_event(int cmd)
{

	switch (cmd) {
	case IPC_RMID:
		return (AUE_MSGCTL_RMID);

	case IPC_SET:
		return (AUE_MSGCTL_SET);

	case IPC_STAT:
		return (AUE_MSGCTL_STAT);

	default:
		/* We will audit a bad command. */
		return (AUE_MSGCTL);
	}
}

/*
 * Convert a SEMCTL command to a specific event.
 */
au_event_t
audit_semctl_to_event(int cmd)
{

	switch (cmd) {
	case GETALL:
		return (AUE_SEMCTL_GETALL);

	case GETNCNT:
		return (AUE_SEMCTL_GETNCNT);

	case GETPID:
		return (AUE_SEMCTL_GETPID);

	case GETVAL:
		return (AUE_SEMCTL_GETVAL);

	case GETZCNT:
		return (AUE_SEMCTL_GETZCNT);

	case IPC_RMID:
		return (AUE_SEMCTL_RMID);

	case IPC_SET:
		return (AUE_SEMCTL_SET);

	case SETALL:
		return (AUE_SEMCTL_SETALL);

	case SETVAL:
		return (AUE_SEMCTL_SETVAL);

	case IPC_STAT:
		return (AUE_SEMCTL_STAT);

	default:
		/* We will audit a bad command. */
		return (AUE_SEMCTL);
	}
}

/*
 * Convert a command for the auditon() system call to a audit event.
 */
au_event_t
auditon_command_event(int cmd)
{

	switch(cmd) {
	case A_GETPOLICY:
		return (AUE_AUDITON_GPOLICY);

	case A_SETPOLICY:
		return (AUE_AUDITON_SPOLICY);

	case A_GETKMASK:
		return (AUE_AUDITON_GETKMASK);

	case A_SETKMASK:
		return (AUE_AUDITON_SETKMASK);

	case A_GETQCTRL:
		return (AUE_AUDITON_GQCTRL);

	case A_SETQCTRL:
		return (AUE_AUDITON_SQCTRL);

	case A_GETCWD:
		return (AUE_AUDITON_GETCWD);

	case A_GETCAR:
		return (AUE_AUDITON_GETCAR);

	case A_GETSTAT:
		return (AUE_AUDITON_GETSTAT);

	case A_SETSTAT:
		return (AUE_AUDITON_SETSTAT);

	case A_SETUMASK:
		return (AUE_AUDITON_SETUMASK);

	case A_SETSMASK:
		return (AUE_AUDITON_SETSMASK);

	case A_GETCOND:
		return (AUE_AUDITON_GETCOND);

	case A_SETCOND:
		return (AUE_AUDITON_SETCOND);

	case A_GETCLASS:
		return (AUE_AUDITON_GETCLASS);

	case A_SETCLASS:
		return (AUE_AUDITON_SETCLASS);

	case A_GETPINFO:
	case A_SETPMASK:
	case A_SETFSIZE:
	case A_GETFSIZE:
	case A_GETPINFO_ADDR:
	case A_GETKAUDIT:
	case A_SETKAUDIT:
	default:
		return (AUE_AUDITON);	/* No special record */
	}
}

/*
 * Create a canonical path from given path by prefixing either the root
 * directory, or the current working directory.  If the process working
 * directory is NULL, we could use 'rootvnode' to obtain the root directory,
 * but this results in a volfs name written to the audit log. So we will
 * leave the filename starting with '/' in the audit log in this case.
 */
void
audit_canon_path(struct thread *td, int dirfd, char *path, char *cpath)
{
	struct vnode *cvnp, *rvnp;
	char *rbuf, *fbuf, *copy;
	struct filedesc *fdp;
	struct sbuf sbf;
	cap_rights_t rights;
	int error, needslash;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "%s: at %s:%d",
	    __func__,  __FILE__, __LINE__);

	copy = path;
	rvnp = cvnp = NULL;
	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	/*
	 * Make sure that we handle the chroot(2) case.  If there is an
	 * alternate root directory, prepend it to the audited pathname.
	 */
	if (fdp->fd_rdir != NULL && fdp->fd_rdir != rootvnode) {
		rvnp = fdp->fd_rdir;
		vhold(rvnp);
	}
	/*
	 * If the supplied path is relative, make sure we capture the current
	 * working directory so we can prepend it to the supplied relative
	 * path.
	 */
	if (*path != '/') {
		if (dirfd == AT_FDCWD) {
			cvnp = fdp->fd_cdir;
			vhold(cvnp);
		} else {
			/* XXX: fgetvp() that vhold()s vnode instead of vref()ing it would be better */
			error = fgetvp(td, dirfd, cap_rights_init(&rights), &cvnp);
			if (error) {
				FILEDESC_SUNLOCK(fdp);
				cpath[0] = '\0';
				if (rvnp != NULL)
					vdrop(rvnp);
				return;
			}
			vhold(cvnp);
			vrele(cvnp);
		}
		needslash = (fdp->fd_rdir != cvnp);
	} else {
		needslash = 1;
	}
	FILEDESC_SUNLOCK(fdp);
	/*
	 * NB: We require that the supplied array be at least MAXPATHLEN bytes
	 * long.  If this is not the case, then we can run into serious trouble.
	 */
	(void) sbuf_new(&sbf, cpath, MAXPATHLEN, SBUF_FIXEDLEN);
	/*
	 * Strip leading forward slashes.
	 */
	while (*copy == '/')
		copy++;
	/*
	 * Make sure we handle chroot(2) and prepend the global path to these
	 * environments.
	 *
	 * NB: vn_fullpath(9) on FreeBSD is less reliable than vn_getpath(9)
	 * on Darwin.  As a result, this may need some additional attention
	 * in the future.
	 */
	if (rvnp != NULL) {
		error = vn_fullpath_global(td, rvnp, &rbuf, &fbuf);
		vdrop(rvnp);
		if (error) {
			cpath[0] = '\0';
			if (cvnp != NULL)
				vdrop(cvnp);
			return;
		}
		(void) sbuf_cat(&sbf, rbuf);
		free(fbuf, M_TEMP);
	}
	if (cvnp != NULL) {
		error = vn_fullpath(td, cvnp, &rbuf, &fbuf);
		vdrop(cvnp);
		if (error) {
			cpath[0] = '\0';
			return;
		}
		(void) sbuf_cat(&sbf, rbuf);
		free(fbuf, M_TEMP);
	}
	if (needslash)
		(void) sbuf_putc(&sbf, '/');
	/*
	 * Now that we have processed any alternate root and relative path
	 * names, add the supplied pathname.
	 */
        (void) sbuf_cat(&sbf, copy);
	/*
	 * One or more of the previous sbuf operations could have resulted in
	 * the supplied buffer being overflowed.  Check to see if this is the
	 * case.
	 */
	if (sbuf_error(&sbf) != 0) {
		cpath[0] = '\0';
		return;
	}
	sbuf_finish(&sbf);
}
