/*
 * Copyright (c) 1999-2005 Apple Computer, Inc.
 * Copyright (c) 2005 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sem.h>
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
#define EVCLASSMAP_HASH_TABLE_SIZE 251
struct evclass_elem {
	au_event_t event;
	au_class_t class;
	LIST_ENTRY(evclass_elem) entry;
};
struct evclass_list {
	LIST_HEAD(, evclass_elem) head;
};

static MALLOC_DEFINE(M_AUDITEVCLASS, "audit_evclass", "Audit event class");
static struct mtx		evclass_mtx;
static struct evclass_list	evclass_hash[EVCLASSMAP_HASH_TABLE_SIZE];

/*
 * Look up the class for an audit event in the class mapping table.
 */
au_class_t
au_event_class(au_event_t event)
{
	struct evclass_list *evcl;
	struct evclass_elem *evc;
	au_class_t class;

	mtx_lock(&evclass_mtx);
	evcl = &evclass_hash[event % EVCLASSMAP_HASH_TABLE_SIZE];
	class = 0;
	LIST_FOREACH(evc, &evcl->head, entry) {
		if (evc->event == event) {
			class = evc->class;
			goto out;
		}
	}
out:
	mtx_unlock(&evclass_mtx);
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

	mtx_lock(&evclass_mtx);
	evcl = &evclass_hash[event % EVCLASSMAP_HASH_TABLE_SIZE];
	LIST_FOREACH(evc, &evcl->head, entry) {
		if (evc->event == event) {
			evc->class = class;
			mtx_unlock(&evclass_mtx);
			free(evc_new, M_AUDITEVCLASS);
			return;
		}
	}
	evc = evc_new;
	evc->event = event;
	evc->class = class;
	LIST_INSERT_HEAD(&evcl->head, evc, entry);
	mtx_unlock(&evclass_mtx);
}

void
au_evclassmap_init(void)
{
	int i;

	mtx_init(&evclass_mtx, "evclass_mtx", NULL, MTX_DEF);
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
	case KERN_MAXID:
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
	au_event_t aevent;

	/*
	 * Need to check only those flags we care about.
	 */
	oflags = oflags & (O_RDONLY | O_CREAT | O_TRUNC | O_RDWR | O_WRONLY);

	/*
	 * These checks determine what flags are on with the condition that
	 * ONLY that combination is on, and no other flags are on.
	 */
	switch (oflags) {
	case O_RDONLY:
		aevent = AUE_OPEN_R;
		break;

	case (O_RDONLY | O_CREAT):
		aevent = AUE_OPEN_RC;
		break;

	case (O_RDONLY | O_CREAT | O_TRUNC):
		aevent = AUE_OPEN_RTC;
		break;

	case (O_RDONLY | O_TRUNC):
		aevent = AUE_OPEN_RT;
		break;

	case O_RDWR:
		aevent = AUE_OPEN_RW;
		break;

	case (O_RDWR | O_CREAT):
		aevent = AUE_OPEN_RWC;
		break;

	case (O_RDWR | O_CREAT | O_TRUNC):
		aevent = AUE_OPEN_RWTC;
		break;

	case (O_RDWR | O_TRUNC):
		aevent = AUE_OPEN_RWT;
		break;

	case O_WRONLY:
		aevent = AUE_OPEN_W;
		break;

	case (O_WRONLY | O_CREAT):
		aevent = AUE_OPEN_WC;
		break;

	case (O_WRONLY | O_CREAT | O_TRUNC):
		aevent = AUE_OPEN_WTC;
		break;

	case (O_WRONLY | O_TRUNC):
		aevent = AUE_OPEN_WT;
		break;

	default:
		aevent = AUE_OPEN;
		break;
	}

#if 0
	/*
	 * Convert chatty errors to better matching events.  Failures to
	 * find a file are really just attribute events -- so recast them as
	 * such.
	 *
	 * XXXAUDIT: Solaris defines that AUE_OPEN will never be returned, it
	 * is just a placeholder.  However, in Darwin we return that in
	 * preference to other events.  For now, comment this out as we don't
	 * have a BSM conversion routine for AUE_OPEN.
	 */
	switch (aevent) {
	case AUE_OPEN_R:
	case AUE_OPEN_RT:
	case AUE_OPEN_RW:
	case AUE_OPEN_RWT:
	case AUE_OPEN_W:
	case AUE_OPEN_WT:
		if (error == ENOENT)
			aevent = AUE_OPEN;
	}
#endif
	return (aevent);
}

/*
 * Convert a MSGCTL command to a specific event.
 */
int
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
int
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
		/* We will audit a bad command */
		return (AUE_SEMCTL);
	}
}

/*
 * Convert a command for the auditon() system call to a audit event.
 */
int
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
 *
 * XXXRW: Since we combine two paths here, ideally a buffer of size
 * MAXPATHLEN * 2 would be passed in.
 */
void
audit_canon_path(struct thread *td, char *path, char *cpath)
{
	char *bufp;
	char *retbuf, *freebuf;
	struct vnode *vnp;
	struct filedesc *fdp;
	int cisr, error, vfslocked;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "audit_canon_path() at %s:%d", __FILE__, __LINE__);

	fdp = td->td_proc->p_fd;
	bufp = path;
	cisr = 0;
	FILEDESC_SLOCK(fdp);
	if (*(path) == '/') {
		while (*(bufp) == '/')
			bufp++;			/* Skip leading '/'s. */
		/*
		 * If no process root, or it is the same as the system root,
		 * audit the path as passed in with a single '/'.
		 */
		if ((fdp->fd_rdir == NULL) ||
		    (fdp->fd_rdir == rootvnode)) {
			vnp = NULL;
			bufp--;			/* Restore one '/'. */
		} else {
			vnp = fdp->fd_rdir;	/* Use process root. */
			vref(vnp);
		}
	} else {
		vnp = fdp->fd_cdir;	/* Prepend the current dir. */
		cisr = (fdp->fd_rdir == fdp->fd_cdir);
		vref(vnp);
		bufp = path;
	}
	FILEDESC_SUNLOCK(fdp);
	if (vnp != NULL) {
		/*
		 * XXX: vn_fullpath() on FreeBSD is "less reliable" than
		 * vn_getpath() on Darwin, so this will need more attention
		 * in the future.  Also, the question and string bounding
		 * here seems a bit questionable and will also require
		 * attention.
		 */
		vfslocked = VFS_LOCK_GIANT(vnp->v_mount);
		vn_lock(vnp, LK_EXCLUSIVE | LK_RETRY, td);
		error = vn_fullpath(td, vnp, &retbuf, &freebuf);
		if (error == 0) {
			/* Copy and free buffer allocated by vn_fullpath().
			 * If the current working directory was the same as
			 * the root directory, and the path was a relative
			 * pathname, do not separate the two components with
			 * the '/' character.
			 */
			snprintf(cpath, MAXPATHLEN, "%s%s%s", retbuf,
			    cisr ? "" : "/", bufp);
			free(freebuf, M_TEMP);
		} else
			cpath[0] = '\0';
		vput(vnp);
		VFS_UNLOCK_GIANT(vfslocked);
	} else
		strlcpy(cpath, bufp, MAXPATHLEN);
}
