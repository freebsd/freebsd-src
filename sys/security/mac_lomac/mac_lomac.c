/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
 */

/*
 * Developed by the TrustedBSD Project.
 * Low-watermark floating label mandatory integrity policy.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/conf.h>
#include <sys/extattr.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/pipe.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

#include <security/mac_lomac/mac_lomac.h>

struct mac_lomac_proc {
	struct mac_lomac mac_lomac;
	struct mtx mtx;
};

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, lomac, CTLFLAG_RW, 0,
    "TrustedBSD mac_lomac policy controls");

static int	mac_lomac_label_size = sizeof(struct mac_lomac);
SYSCTL_INT(_security_mac_lomac, OID_AUTO, label_size, CTLFLAG_RD,
    &mac_lomac_label_size, 0, "Size of struct mac_lomac");

static int	mac_lomac_enabled = 1;
SYSCTL_INT(_security_mac_lomac, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_lomac_enabled, 0, "Enforce MAC/LOMAC policy");
TUNABLE_INT("security.mac.lomac.enabled", &mac_lomac_enabled);

static int	destroyed_not_inited;
SYSCTL_INT(_security_mac_lomac, OID_AUTO, destroyed_not_inited, CTLFLAG_RD,
    &destroyed_not_inited, 0, "Count of labels destroyed but not inited");

static int	trust_all_interfaces = 0;
SYSCTL_INT(_security_mac_lomac, OID_AUTO, trust_all_interfaces, CTLFLAG_RD,
    &trust_all_interfaces, 0, "Consider all interfaces 'trusted' by MAC/LOMAC");
TUNABLE_INT("security.mac.lomac.trust_all_interfaces", &trust_all_interfaces);

static char	trusted_interfaces[128];
SYSCTL_STRING(_security_mac_lomac, OID_AUTO, trusted_interfaces, CTLFLAG_RD,
    trusted_interfaces, 0, "Interfaces considered 'trusted' by MAC/LOMAC");
TUNABLE_STR("security.mac.lomac.trusted_interfaces", trusted_interfaces,
    sizeof(trusted_interfaces));

static int	ptys_equal = 0;
SYSCTL_INT(_security_mac_lomac, OID_AUTO, ptys_equal, CTLFLAG_RW,
    &ptys_equal, 0, "Label pty devices as lomac/equal on create");
TUNABLE_INT("security.mac.lomac.ptys_equal", &ptys_equal);

static int	revocation_enabled = 1;
SYSCTL_INT(_security_mac_lomac, OID_AUTO, revocation_enabled, CTLFLAG_RW,
    &revocation_enabled, 0, "Revoke access to objects on relabel");
TUNABLE_INT("security.mac.lomac.revocation_enabled", &revocation_enabled);

static int	mac_lomac_slot;
#define	SLOT(l)	((struct mac_lomac *)LABEL_TO_SLOT((l), mac_lomac_slot).l_ptr)
#define	PSLOT(l) ((struct mac_lomac_proc *)				\
    LABEL_TO_SLOT((l), mac_lomac_slot).l_ptr)

MALLOC_DEFINE(M_MACLOMAC, "lomac label", "MAC/LOMAC labels");

static struct mac_lomac *
lomac_alloc(int flag)
{
	struct mac_lomac *mac_lomac;

	mac_lomac = malloc(sizeof(struct mac_lomac), M_MACLOMAC, M_ZERO | flag);

	return (mac_lomac);
}

static void
lomac_free(struct mac_lomac *mac_lomac)
{

	if (mac_lomac != NULL)
		free(mac_lomac, M_MACLOMAC);
	else
		atomic_add_int(&destroyed_not_inited, 1);
}

static int
lomac_atmostflags(struct mac_lomac *mac_lomac, int flags)
{

	if ((mac_lomac->ml_flags & flags) != mac_lomac->ml_flags)
		return (EINVAL);
	return (0);
}

static int
mac_lomac_dominate_element(struct mac_lomac_element *a,
    struct mac_lomac_element *b)
{

	switch (a->mle_type) {
	case MAC_LOMAC_TYPE_EQUAL:
	case MAC_LOMAC_TYPE_HIGH:
		return (1);

	case MAC_LOMAC_TYPE_LOW:
		switch (b->mle_type) {
		case MAC_LOMAC_TYPE_GRADE:
		case MAC_LOMAC_TYPE_HIGH:
			return (0);

		case MAC_LOMAC_TYPE_EQUAL:
		case MAC_LOMAC_TYPE_LOW:
			return (1);

		default:
			panic("mac_lomac_dominate_element: b->mle_type invalid");
		}

	case MAC_LOMAC_TYPE_GRADE:
		switch (b->mle_type) {
		case MAC_LOMAC_TYPE_EQUAL:
		case MAC_LOMAC_TYPE_LOW:
			return (1);

		case MAC_LOMAC_TYPE_HIGH:
			return (0);

		case MAC_LOMAC_TYPE_GRADE:
			return (a->mle_grade >= b->mle_grade);

		default:
			panic("mac_lomac_dominate_element: b->mle_type invalid");
		}

	default:
		panic("mac_lomac_dominate_element: a->mle_type invalid");
	}
}

static int
mac_lomac_range_in_range(struct mac_lomac *rangea, struct mac_lomac *rangeb)
{

	return (mac_lomac_dominate_element(&rangeb->ml_rangehigh,
	    &rangea->ml_rangehigh) &&
	    mac_lomac_dominate_element(&rangea->ml_rangelow,
	    &rangeb->ml_rangelow));
}

static int
mac_lomac_single_in_range(struct mac_lomac *single, struct mac_lomac *range)
{

	KASSERT((single->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_single_in_range: a not single"));
	KASSERT((range->ml_flags & MAC_LOMAC_FLAG_RANGE) != 0,
	    ("mac_lomac_single_in_range: b not range"));

	return (mac_lomac_dominate_element(&range->ml_rangehigh,
	    &single->ml_single) &&
	    mac_lomac_dominate_element(&single->ml_single,
	    &range->ml_rangelow));
}

static int
mac_lomac_auxsingle_in_range(struct mac_lomac *single, struct mac_lomac *range)
{

	KASSERT((single->ml_flags & MAC_LOMAC_FLAG_AUX) != 0,
	    ("mac_lomac_single_in_range: a not auxsingle"));
	KASSERT((range->ml_flags & MAC_LOMAC_FLAG_RANGE) != 0,
	    ("mac_lomac_single_in_range: b not range"));

	return (mac_lomac_dominate_element(&range->ml_rangehigh,
	    &single->ml_auxsingle) &&
	    mac_lomac_dominate_element(&single->ml_auxsingle,
	    &range->ml_rangelow));

	return (1);
}

static int
mac_lomac_dominate_single(struct mac_lomac *a, struct mac_lomac *b)
{
	KASSERT((a->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_dominate_single: a not single"));
	KASSERT((b->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_dominate_single: b not single"));

	return (mac_lomac_dominate_element(&a->ml_single, &b->ml_single));
}

static int
mac_lomac_subject_dominate(struct mac_lomac *a, struct mac_lomac *b)
{
	KASSERT((~a->ml_flags &
	    (MAC_LOMAC_FLAG_SINGLE | MAC_LOMAC_FLAG_RANGE)) == 0,
	    ("mac_lomac_dominate_single: a not subject"));
	KASSERT((b->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_dominate_single: b not single"));

	return (mac_lomac_dominate_element(&a->ml_rangehigh,
	    &b->ml_single));
}

static int
mac_lomac_equal_element(struct mac_lomac_element *a, struct mac_lomac_element *b)
{

	if (a->mle_type == MAC_LOMAC_TYPE_EQUAL ||
	    b->mle_type == MAC_LOMAC_TYPE_EQUAL)
		return (1);

	return (a->mle_type == b->mle_type && a->mle_grade == b->mle_grade);
}

static int
mac_lomac_equal_single(struct mac_lomac *a, struct mac_lomac *b)
{

	KASSERT((a->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_equal_single: a not single"));
	KASSERT((b->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_equal_single: b not single"));

	return (mac_lomac_equal_element(&a->ml_single, &b->ml_single));
}

static int
mac_lomac_contains_equal(struct mac_lomac *mac_lomac)
{

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_SINGLE)
		if (mac_lomac->ml_single.mle_type == MAC_LOMAC_TYPE_EQUAL)
			return (1);
	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_AUX)
		if (mac_lomac->ml_auxsingle.mle_type == MAC_LOMAC_TYPE_EQUAL)
			return (1);

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_RANGE) {
		if (mac_lomac->ml_rangelow.mle_type == MAC_LOMAC_TYPE_EQUAL)
			return (1);
		if (mac_lomac->ml_rangehigh.mle_type == MAC_LOMAC_TYPE_EQUAL)
			return (1);
	}

	return (0);
}

static int
mac_lomac_subject_privileged(struct mac_lomac *mac_lomac)
{

	KASSERT((mac_lomac->ml_flags & MAC_LOMAC_FLAGS_BOTH) ==
	    MAC_LOMAC_FLAGS_BOTH,
	    ("mac_lomac_subject_privileged: subject doesn't have both labels"));

	/* If the single is EQUAL, it's ok. */
	if (mac_lomac->ml_single.mle_type == MAC_LOMAC_TYPE_EQUAL)
		return (0);

	/* If either range endpoint is EQUAL, it's ok. */
	if (mac_lomac->ml_rangelow.mle_type == MAC_LOMAC_TYPE_EQUAL ||
	    mac_lomac->ml_rangehigh.mle_type == MAC_LOMAC_TYPE_EQUAL)
		return (0);

	/* If the range is low-high, it's ok. */
	if (mac_lomac->ml_rangelow.mle_type == MAC_LOMAC_TYPE_LOW &&
	    mac_lomac->ml_rangehigh.mle_type == MAC_LOMAC_TYPE_HIGH)
		return (0);

	/* It's not ok. */
	return (EPERM);
}

static int
mac_lomac_high_single(struct mac_lomac *mac_lomac)
{

	KASSERT((mac_lomac->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_high_single: mac_lomac not single"));
 
	return (mac_lomac->ml_single.mle_type == MAC_LOMAC_TYPE_HIGH);
}

static int
mac_lomac_valid(struct mac_lomac *mac_lomac)
{

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_SINGLE) {
		switch (mac_lomac->ml_single.mle_type) {
		case MAC_LOMAC_TYPE_GRADE:
		case MAC_LOMAC_TYPE_EQUAL:
		case MAC_LOMAC_TYPE_HIGH:
		case MAC_LOMAC_TYPE_LOW:
			break;

		default:
			return (EINVAL);
		}
	} else {
		if (mac_lomac->ml_single.mle_type != MAC_LOMAC_TYPE_UNDEF)
			return (EINVAL);
	}

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_AUX) {
		switch (mac_lomac->ml_auxsingle.mle_type) {
		case MAC_LOMAC_TYPE_GRADE:
		case MAC_LOMAC_TYPE_EQUAL:
		case MAC_LOMAC_TYPE_HIGH:
		case MAC_LOMAC_TYPE_LOW:
			break;

		default:
			return (EINVAL);
		}
	} else {
		if (mac_lomac->ml_auxsingle.mle_type != MAC_LOMAC_TYPE_UNDEF)
			return (EINVAL);
	}

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_RANGE) {
		switch (mac_lomac->ml_rangelow.mle_type) {
		case MAC_LOMAC_TYPE_GRADE:
		case MAC_LOMAC_TYPE_EQUAL:
		case MAC_LOMAC_TYPE_HIGH:
		case MAC_LOMAC_TYPE_LOW:
			break;

		default:
			return (EINVAL);
		}

		switch (mac_lomac->ml_rangehigh.mle_type) {
		case MAC_LOMAC_TYPE_GRADE:
		case MAC_LOMAC_TYPE_EQUAL:
		case MAC_LOMAC_TYPE_HIGH:
		case MAC_LOMAC_TYPE_LOW:
			break;

		default:
			return (EINVAL);
		}
		if (!mac_lomac_dominate_element(&mac_lomac->ml_rangehigh,
		    &mac_lomac->ml_rangelow))
			return (EINVAL);
	} else {
		if (mac_lomac->ml_rangelow.mle_type != MAC_LOMAC_TYPE_UNDEF ||
		    mac_lomac->ml_rangehigh.mle_type != MAC_LOMAC_TYPE_UNDEF)
			return (EINVAL);
	}

	return (0);
}

static void
mac_lomac_set_range(struct mac_lomac *mac_lomac, u_short typelow,
    u_short gradelow, u_short typehigh, u_short gradehigh)
{

	mac_lomac->ml_rangelow.mle_type = typelow;
	mac_lomac->ml_rangelow.mle_grade = gradelow;
	mac_lomac->ml_rangehigh.mle_type = typehigh;
	mac_lomac->ml_rangehigh.mle_grade = gradehigh;
	mac_lomac->ml_flags |= MAC_LOMAC_FLAG_RANGE;
}

static void
mac_lomac_set_single(struct mac_lomac *mac_lomac, u_short type, u_short grade)
{

	mac_lomac->ml_single.mle_type = type;
	mac_lomac->ml_single.mle_grade = grade;
	mac_lomac->ml_flags |= MAC_LOMAC_FLAG_SINGLE;
}

static void
mac_lomac_copy_range(struct mac_lomac *labelfrom, struct mac_lomac *labelto)
{

	KASSERT((labelfrom->ml_flags & MAC_LOMAC_FLAG_RANGE) != 0,
	    ("mac_lomac_copy_range: labelfrom not range"));

	labelto->ml_rangelow = labelfrom->ml_rangelow;
	labelto->ml_rangehigh = labelfrom->ml_rangehigh;
	labelto->ml_flags |= MAC_LOMAC_FLAG_RANGE;
}

static void
mac_lomac_copy_single(struct mac_lomac *labelfrom, struct mac_lomac *labelto)
{

	KASSERT((labelfrom->ml_flags & MAC_LOMAC_FLAG_SINGLE) != 0,
	    ("mac_lomac_copy_single: labelfrom not single"));

	labelto->ml_single = labelfrom->ml_single;
	labelto->ml_flags |= MAC_LOMAC_FLAG_SINGLE;
}

static void
mac_lomac_copy_auxsingle(struct mac_lomac *labelfrom, struct mac_lomac *labelto)
{

	KASSERT((labelfrom->ml_flags & MAC_LOMAC_FLAG_AUX) != 0,
	    ("mac_lomac_copy_auxsingle: labelfrom not auxsingle"));

	labelto->ml_auxsingle = labelfrom->ml_auxsingle;
	labelto->ml_flags |= MAC_LOMAC_FLAG_AUX;
}

static void
mac_lomac_copy(struct mac_lomac *source, struct mac_lomac *dest)
{

	if (source->ml_flags & MAC_LOMAC_FLAG_SINGLE)
		mac_lomac_copy_single(source, dest);
	if (source->ml_flags & MAC_LOMAC_FLAG_AUX)
		mac_lomac_copy_auxsingle(source, dest);
	if (source->ml_flags & MAC_LOMAC_FLAG_RANGE)
		mac_lomac_copy_range(source, dest);
}

static int	mac_lomac_to_string(char *string, size_t size,
	    size_t *caller_len, struct mac_lomac *mac_lomac);

static int
maybe_demote(struct mac_lomac *subjlabel, struct mac_lomac *objlabel,
    const char *actionname, const char *objname, struct vnode *vpq)
{
	struct vattr va;
	static char xxx[] = "<<XXX>>";
	struct mac_lomac_proc *subj = PSLOT(&curthread->td_proc->p_label);
	char *subjlabeltext, *objlabeltext, *subjtext, *text;
	struct proc *p;
	size_t len;
	pid_t pgid;

	p = curthread->td_proc;
	mtx_lock(&subj->mtx);
        if (subj->mac_lomac.ml_flags & MAC_LOMAC_FLAG_UPDATE) {
		/*
		 * Check to see if the pending demotion would be more or
		 * less severe than this one, and keep the more severe.
		 * This can only happen for a multi-threaded application.
		 */
		if (mac_lomac_dominate_single(objlabel, &subj->mac_lomac)) {
			mtx_unlock(&subj->mtx);
			return (0);
		}
	}
	bzero(&subj->mac_lomac, sizeof(subj->mac_lomac));
	/*
	 * Always demote the single label.
	 */
	mac_lomac_copy_single(objlabel, &subj->mac_lomac);
	/*
	 * Start with the original range, then minimize each side of
	 * the range to the point of not dominating the object.  The
	 * high side will always be demoted, of course.
	 */
	mac_lomac_copy_range(subjlabel, &subj->mac_lomac);
	if (!mac_lomac_dominate_element(&objlabel->ml_single,
	    &subj->mac_lomac.ml_rangelow))
		subj->mac_lomac.ml_rangelow = objlabel->ml_single;
	subj->mac_lomac.ml_rangehigh = objlabel->ml_single;
	subj->mac_lomac.ml_flags |= MAC_LOMAC_FLAG_UPDATE;
	mtx_lock_spin(&sched_lock);
	curthread->td_kse->ke_flags |= KEF_ASTPENDING;
	curthread->td_proc->p_sflag |= PS_MACPEND;
	mtx_unlock_spin(&sched_lock);
	subjtext = subjlabeltext = objlabeltext = xxx;
	if (mac_lomac_to_string(NULL, 0, &len, &subj->mac_lomac) == 0 &&
	    (text = malloc(len + 1, M_MACLOMAC, M_NOWAIT)) != NULL) {
		if (mac_lomac_to_string(text, len + 1, &len,
		    &subj->mac_lomac) == 0)
			subjtext = text;
		else
			free(text, M_MACLOMAC);
	}
	mtx_unlock(&subj->mtx);
	if (mac_lomac_to_string(NULL, 0, &len, subjlabel) == 0 &&
	    (text = malloc(len + 1, M_MACLOMAC, M_NOWAIT)) != NULL) {
		if (mac_lomac_to_string(text, len + 1, &len,
		    subjlabel) == 0)
			subjlabeltext = text;
		else
			free(text, M_MACLOMAC);
	}
	if (mac_lomac_to_string(NULL, 0, &len, objlabel) == 0 &&
	    (text = malloc(len + 1, M_MACLOMAC, M_NOWAIT)) != NULL) {
		if (mac_lomac_to_string(text, len + 1, &len,
		    objlabel) == 0)
			objlabeltext = text;
		else
			free(text, M_MACLOMAC);
	}
	pgid = p->p_pgrp->pg_id;		/* XXX could be stale? */
	if (vpq != NULL && VOP_GETATTR(vpq, &va, curthread->td_ucred,
	    curthread) == 0) {
		log(LOG_INFO, "LOMAC: level-%s subject p%dg%du%d:%s demoted to"
		    " level %s after %s a level-%s %s (inode=%ld, "
		    "mountpount=%s)\n",
		    subjlabeltext, p->p_pid, pgid, curthread->td_ucred->cr_uid,
		    p->p_comm, subjtext, actionname, objlabeltext, objname,
		    va.va_fileid, vpq->v_mount->mnt_stat.f_mntonname);
	} else {
		log(LOG_INFO, "LOMAC: level-%s subject p%dg%du%d:%s demoted to"
		    " level %s after %s a level-%s %s\n",
		    subjlabeltext, p->p_pid, pgid, curthread->td_ucred->cr_uid,
		    p->p_comm, subjtext, actionname, objlabeltext, objname);
	}
		
	if (subjlabeltext != xxx)
		free(subjlabeltext, M_MACLOMAC);
	if (objlabeltext != xxx)
		free(objlabeltext, M_MACLOMAC);
	if (subjtext != xxx)
		free(subjtext, M_MACLOMAC);
	return (0);
}

/*
 * Relabel "to" to "from" only if "from" is a valid label (contains
 * at least a single), as for a relabel operation which may or may
 * not involve a relevant label.
 */
static void
try_relabel(struct mac_lomac *from, struct mac_lomac *to)
{

	if (from->ml_flags & MAC_LOMAC_FLAG_SINGLE) {
		bzero(to, sizeof(*to));
		mac_lomac_copy(from, to);
	}
}

/*
 * Policy module operations.
 */
static void
mac_lomac_destroy(struct mac_policy_conf *conf)
{

}

static void
mac_lomac_init(struct mac_policy_conf *conf)
{

}

/*
 * Label operations.
 */
static void
mac_lomac_init_label(struct label *label)
{

	SLOT(label) = lomac_alloc(M_WAITOK);
}

static int
mac_lomac_init_label_waitcheck(struct label *label, int flag)
{

	SLOT(label) = lomac_alloc(flag);
	if (SLOT(label) == NULL)
		return (ENOMEM);

	return (0);
}

static void
mac_lomac_init_proc_label(struct label *label)
{

	PSLOT(label) = malloc(sizeof(struct mac_lomac_proc), M_MACLOMAC,
	    M_ZERO | M_WAITOK);
	mtx_init(&PSLOT(label)->mtx, "MAC/Lomac proc lock", NULL, MTX_DEF);
}

static void
mac_lomac_destroy_label(struct label *label)
{

	lomac_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_lomac_destroy_proc_label(struct label *label)
{

	mtx_destroy(&PSLOT(label)->mtx);
	FREE(PSLOT(label), M_MACLOMAC);
	PSLOT(label) = NULL;
}

/*
 * mac_lomac_element_to_string() is basically an snprintf wrapper with
 * the same properties as snprintf().  It returns the length it would
 * have added to the string in the event the string is too short.
 */
static size_t
mac_lomac_element_to_string(char *string, size_t size,
    struct mac_lomac_element *element)
{

	switch (element->mle_type) {
	case MAC_LOMAC_TYPE_HIGH:
		return (snprintf(string, size, "high"));

	case MAC_LOMAC_TYPE_LOW:
		return (snprintf(string, size, "low"));

	case MAC_LOMAC_TYPE_EQUAL:
		return (snprintf(string, size, "equal"));

	case MAC_LOMAC_TYPE_GRADE:
		return (snprintf(string, size, "%d", element->mle_grade));

	default:
		panic("mac_lomac_element_to_string: invalid type (%d)",
		    element->mle_type);
	}
}

static int
mac_lomac_to_string(char *string, size_t size, size_t *caller_len,
    struct mac_lomac *mac_lomac)
{
	size_t left, len, curlen;
	char *curptr;

	/*
	 * Also accept NULL string to allow for predetermination of total
	 * string length.
	 */
	if (string != NULL)
		bzero(string, size);
	else if (size != 0)
		return (EINVAL);
	curptr = string;
	left = size;
	curlen = 0;

#define	INCLEN(length, leftover) do {					\
	if (string != NULL) {						\
		if (length >= leftover)					\
			return (EINVAL);				\
		leftover -= length;					\
		curptr += length;					\
	}								\
	curlen += length;						\
} while (0)
	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_SINGLE) {
		len = mac_lomac_element_to_string(curptr, left,
		    &mac_lomac->ml_single);
		INCLEN(len, left);
	}

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_AUX) {
		len = snprintf(curptr, left, "[");
		INCLEN(len, left);

		len = mac_lomac_element_to_string(curptr, left,
		    &mac_lomac->ml_auxsingle);
		INCLEN(len, left);

		len = snprintf(curptr, left, "]");
		INCLEN(len, left);
	}

	if (mac_lomac->ml_flags & MAC_LOMAC_FLAG_RANGE) {
		len = snprintf(curptr, left, "(");
		INCLEN(len, left);

		len = mac_lomac_element_to_string(curptr, left,
		    &mac_lomac->ml_rangelow);
		INCLEN(len, left);

		len = snprintf(curptr, left, "-");
		INCLEN(len, left);

		len = mac_lomac_element_to_string(curptr, left,
		    &mac_lomac->ml_rangehigh);
		INCLEN(len, left);

		len = snprintf(curptr, left, ")");
		INCLEN(len, left);
	}
#undef INCLEN

	*caller_len = curlen;
	return (0);
}

static int
mac_lomac_externalize_label(struct label *label, char *element_name,
    char *element_data, size_t size, size_t *len, int *claimed)
{
	struct mac_lomac *mac_lomac;
	int error;

	if (strcmp(MAC_LOMAC_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;

	mac_lomac = SLOT(label);
	error = mac_lomac_to_string(element_data, size, len, mac_lomac);
	if (error)
		return (error);

	*len = strlen(element_data);
	return (0);
}

static int
mac_lomac_parse_element(struct mac_lomac_element *element, char *string)
{

	if (strcmp(string, "high") == 0 ||
	    strcmp(string, "hi") == 0) {
		element->mle_type = MAC_LOMAC_TYPE_HIGH;
		element->mle_grade = MAC_LOMAC_TYPE_UNDEF;
	} else if (strcmp(string, "low") == 0 ||
	    strcmp(string, "lo") == 0) {
		element->mle_type = MAC_LOMAC_TYPE_LOW;
		element->mle_grade = MAC_LOMAC_TYPE_UNDEF;
	} else if (strcmp(string, "equal") == 0 ||
	    strcmp(string, "eq") == 0) {
		element->mle_type = MAC_LOMAC_TYPE_EQUAL;
		element->mle_grade = MAC_LOMAC_TYPE_UNDEF;
	} else {
		char *p0, *p1;
		int d;

		p0 = string;
		d = strtol(p0, &p1, 10);
	
		if (d < 0 || d > 65535)
			return (EINVAL);
		element->mle_type = MAC_LOMAC_TYPE_GRADE;
		element->mle_grade = d;

		if (p1 == p0 || *p1 != '\0')
			return (EINVAL);
	}

	return (0);
}

/*
 * Note: destructively consumes the string, make a local copy before
 * calling if that's a problem.
 */
static int
mac_lomac_parse(struct mac_lomac *mac_lomac, char *string)
{
	char *range, *rangeend, *rangehigh, *rangelow, *single, *auxsingle,
	    *auxsingleend;
	int error;

	/* Do we have a range? */
	single = string;
	range = index(string, '(');
	if (range == single)
		single = NULL;
	auxsingle = index(string, '[');
	if (auxsingle == single)
		single = NULL;
	if (range != NULL && auxsingle != NULL)
		return (EINVAL);
	rangelow = rangehigh = NULL;
	if (range != NULL) {
		/* Nul terminate the end of the single string. */
		*range = '\0';
		range++;
		rangelow = range;
		rangehigh = index(rangelow, '-');
		if (rangehigh == NULL)
			return (EINVAL);
		rangehigh++;
		if (*rangelow == '\0' || *rangehigh == '\0')
			return (EINVAL);
		rangeend = index(rangehigh, ')');
		if (rangeend == NULL)
			return (EINVAL);
		if (*(rangeend + 1) != '\0')
			return (EINVAL);
		/* Nul terminate the ends of the ranges. */
		*(rangehigh - 1) = '\0';
		*rangeend = '\0';
	}
	KASSERT((rangelow != NULL && rangehigh != NULL) ||
	    (rangelow == NULL && rangehigh == NULL),
	    ("mac_lomac_internalize_label: range mismatch"));
	if (auxsingle != NULL) {
		/* Nul terminate the end of the single string. */
		*auxsingle = '\0';
		auxsingle++;
		auxsingleend = index(auxsingle, ']');
		if (auxsingleend == NULL)
			return (EINVAL);
		if (*(auxsingleend + 1) != '\0')
			return (EINVAL);
		/* Nul terminate the end of the auxsingle. */
		*auxsingleend = '\0';
	}

	bzero(mac_lomac, sizeof(*mac_lomac));
	if (single != NULL) {
		error = mac_lomac_parse_element(&mac_lomac->ml_single, single);
		if (error)
			return (error);
		mac_lomac->ml_flags |= MAC_LOMAC_FLAG_SINGLE;
	}

	if (auxsingle != NULL) {
		error = mac_lomac_parse_element(&mac_lomac->ml_auxsingle,
		    auxsingle);
		if (error)
			return (error);
		mac_lomac->ml_flags |= MAC_LOMAC_FLAG_AUX;
	}

	if (rangelow != NULL) {
		error = mac_lomac_parse_element(&mac_lomac->ml_rangelow,
		    rangelow);
		if (error)
			return (error);
		error = mac_lomac_parse_element(&mac_lomac->ml_rangehigh,
		    rangehigh);
		if (error)
			return (error);
		mac_lomac->ml_flags |= MAC_LOMAC_FLAG_RANGE;
	}

	error = mac_lomac_valid(mac_lomac);
	if (error)
		return (error);

	return (0);
}

static int
mac_lomac_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{
	struct mac_lomac *mac_lomac, mac_lomac_temp;
	int error;

	if (strcmp(MAC_LOMAC_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;

	error = mac_lomac_parse(&mac_lomac_temp, element_data);
	if (error)
		return (error);

	mac_lomac = SLOT(label);
	*mac_lomac = mac_lomac_temp;

	return (0);
}

static void
mac_lomac_copy_label(struct label *src, struct label *dest)
{

	*SLOT(dest) = *SLOT(src);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
static void
mac_lomac_create_devfs_device(struct mount *mp, dev_t dev,
    struct devfs_dirent *devfs_dirent, struct label *label)
{
	struct mac_lomac *mac_lomac;
	int lomac_type;

	mac_lomac = SLOT(label);
	if (strcmp(dev->si_name, "null") == 0 ||
	    strcmp(dev->si_name, "zero") == 0 ||
	    strcmp(dev->si_name, "random") == 0 ||
	    strncmp(dev->si_name, "fd/", strlen("fd/")) == 0 ||
	    strncmp(dev->si_name, "ttyv", strlen("ttyv")) == 0)
		lomac_type = MAC_LOMAC_TYPE_EQUAL;
	else if (ptys_equal &&
	    (strncmp(dev->si_name, "ttyp", strlen("ttyp")) == 0 ||
	    strncmp(dev->si_name, "ptyp", strlen("ptyp")) == 0))
		lomac_type = MAC_LOMAC_TYPE_EQUAL;
	else
		lomac_type = MAC_LOMAC_TYPE_HIGH;
	mac_lomac_set_single(mac_lomac, lomac_type, 0);
}

static void
mac_lomac_create_devfs_directory(struct mount *mp, char *dirname,
    int dirnamelen, struct devfs_dirent *devfs_dirent, struct label *label)
{
	struct mac_lomac *mac_lomac;

	mac_lomac = SLOT(label);
	mac_lomac_set_single(mac_lomac, MAC_LOMAC_TYPE_HIGH, 0);
}

static void
mac_lomac_create_devfs_symlink(struct ucred *cred, struct mount *mp,
    struct devfs_dirent *dd, struct label *ddlabel, struct devfs_dirent *de,
    struct label *delabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(delabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(mntlabel);
	mac_lomac_copy_single(source, dest);
	dest = SLOT(fslabel);
	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_root_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{
	struct mac_lomac *mac_lomac;

	/* Always mount root as high integrity. */
	mac_lomac = SLOT(fslabel);
	mac_lomac_set_single(mac_lomac, MAC_LOMAC_TYPE_HIGH, 0);
	mac_lomac = SLOT(mntlabel);
	mac_lomac_set_single(mac_lomac, MAC_LOMAC_TYPE_HIGH, 0);
}

static void
mac_lomac_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *label)
{
	struct mac_lomac *source, *dest;

	source = SLOT(label);
	dest = SLOT(vnodelabel);

	try_relabel(source, dest);
}

static void
mac_lomac_update_devfsdirent(struct mount *mp,
    struct devfs_dirent *devfs_dirent, struct label *direntlabel,
    struct vnode *vp, struct label *vnodelabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(vnodelabel);
	dest = SLOT(direntlabel);

	mac_lomac_copy(source, dest);
}

static void
mac_lomac_associate_vnode_devfs(struct mount *mp, struct label *fslabel,
    struct devfs_dirent *de, struct label *delabel, struct vnode *vp,
    struct label *vlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(delabel);
	dest = SLOT(vlabel);

	mac_lomac_copy_single(source, dest);
}

static int
mac_lomac_associate_vnode_extattr(struct mount *mp, struct label *fslabel,
    struct vnode *vp, struct label *vlabel)
{
	struct mac_lomac temp, *source, *dest;
	size_t buflen;
	int error;

	source = SLOT(fslabel);
	dest = SLOT(vlabel);

	buflen = sizeof(temp);
	bzero(&temp, buflen);

	error = vn_extattr_get(vp, IO_NODELOCKED, MAC_LOMAC_EXTATTR_NAMESPACE,
	    MAC_LOMAC_EXTATTR_NAME, &buflen, (char *)&temp, curthread);
	if (error == ENOATTR || error == EOPNOTSUPP) {
		/* Fall back to the fslabel. */
		mac_lomac_copy_single(source, dest);
		return (0);
	} else if (error)
		return (error);

	if (buflen != sizeof(temp)) {
		if (buflen != sizeof(temp) - sizeof(temp.ml_auxsingle)) {
			printf("mac_lomac_associate_vnode_extattr: bad size %d\n",
			    buflen);
			return (EPERM);
		}
		bzero(&temp.ml_auxsingle, sizeof(temp.ml_auxsingle));
		buflen = sizeof(temp);
		(void)vn_extattr_set(vp, IO_NODELOCKED,
		    MAC_LOMAC_EXTATTR_NAMESPACE, MAC_LOMAC_EXTATTR_NAME,
		    buflen, (char *)&temp, curthread);
	}
	if (mac_lomac_valid(&temp) != 0) {
		printf("mac_lomac_associate_vnode_extattr: invalid\n");
		return (EPERM);
	}
	if ((temp.ml_flags & MAC_LOMAC_FLAGS_BOTH) != MAC_LOMAC_FLAG_SINGLE) {
		printf("mac_lomac_associate_vnode_extattr: not single\n");
		return (EPERM);
	}

	mac_lomac_copy_single(&temp, dest);
	return (0);
}

static void
mac_lomac_associate_vnode_singlelabel(struct mount *mp,
    struct label *fslabel, struct vnode *vp, struct label *vlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(fslabel);
	dest = SLOT(vlabel);

	mac_lomac_copy_single(source, dest);
}

static int
mac_lomac_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct label *fslabel, struct vnode *dvp, struct label *dlabel,
    struct vnode *vp, struct label *vlabel, struct componentname *cnp)
{
	struct mac_lomac *source, *dest, *dir, temp;
	size_t buflen;
	int error;

	buflen = sizeof(temp);
	bzero(&temp, buflen);

	source = SLOT(&cred->cr_label);
	dest = SLOT(vlabel);
	dir = SLOT(dlabel);
	if (dir->ml_flags & MAC_LOMAC_FLAG_AUX) {
		mac_lomac_copy_auxsingle(dir, &temp);
		mac_lomac_set_single(&temp, dir->ml_auxsingle.mle_type,
		    dir->ml_auxsingle.mle_grade);
	} else {
		mac_lomac_copy_single(source, &temp);
	}

	error = vn_extattr_set(vp, IO_NODELOCKED, MAC_LOMAC_EXTATTR_NAMESPACE,
	    MAC_LOMAC_EXTATTR_NAME, buflen, (char *)&temp, curthread);
	if (error == 0)
		mac_lomac_copy(&temp, dest);
	return (error);
}

static int
mac_lomac_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *vlabel, struct label *intlabel)
{
	struct mac_lomac *source, temp;
	size_t buflen;
	int error;

	buflen = sizeof(temp);
	bzero(&temp, buflen);

	source = SLOT(intlabel);
	if ((source->ml_flags & MAC_LOMAC_FLAG_SINGLE) == 0)
		return (0);

	mac_lomac_copy_single(source, &temp);
	error = vn_extattr_set(vp, IO_NODELOCKED, MAC_LOMAC_EXTATTR_NAMESPACE,
	    MAC_LOMAC_EXTATTR_NAME, buflen, (char *)&temp, curthread);
	return (error);
}

/*
 * Labeling event operations: IPC object.
 */
static void
mac_lomac_create_mbuf_from_socket(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(socketlabel);
	dest = SLOT(mbuflabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(socketlabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(pipelabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_socket_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(oldsocketlabel);
	dest = SLOT(newsocketlabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(socketlabel);

	try_relabel(source, dest);
}

static void
mac_lomac_relabel_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(pipelabel);

	try_relabel(source, dest);
}

static void
mac_lomac_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct label *mbuflabel,
    struct socket *socket, struct label *socketpeerlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(mbuflabel);
	dest = SLOT(socketpeerlabel);

	mac_lomac_copy_single(source, dest);
}

/*
 * Labeling event operations: network objects.
 */
static void
mac_lomac_set_socket_peer_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketpeerlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(oldsocketlabel);
	dest = SLOT(newsocketpeerlabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d,
    struct label *bpflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(bpflabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_ifnet(struct ifnet *ifnet, struct label *ifnetlabel)
{
	char tifname[IFNAMSIZ], ifname[IFNAMSIZ], *p, *q;
	char tiflist[sizeof(trusted_interfaces)];
	struct mac_lomac *dest;
	int len, grade;

	dest = SLOT(ifnetlabel);

	if (ifnet->if_type == IFT_LOOP) {
		grade = MAC_LOMAC_TYPE_EQUAL;
		goto set;
	}

	if (trust_all_interfaces) {
		grade = MAC_LOMAC_TYPE_HIGH;
		goto set;
	}

	grade = MAC_LOMAC_TYPE_LOW;

	if (trusted_interfaces[0] == '\0' ||
	    !strvalid(trusted_interfaces, sizeof(trusted_interfaces)))
		goto set;

	bzero(tiflist, sizeof(tiflist));
	for (p = trusted_interfaces, q = tiflist; *p != '\0'; p++, q++)
		if(*p != ' ' && *p != '\t')
			*q = *p;

	snprintf(ifname, IFNAMSIZ, "%s%d", ifnet->if_name, ifnet->if_unit);

	for (p = q = tiflist;; p++) {
		if (*p == ',' || *p == '\0') {
			len = p - q;
			if (len < IFNAMSIZ) {
				bzero(tifname, sizeof(tifname));
				bcopy(q, tifname, len);
				if (strcmp(tifname, ifname) == 0) {
					grade = MAC_LOMAC_TYPE_HIGH;
					break;
				}
			}
			else {
				*p = '\0';
				printf("MAC/LOMAC warning: interface name "
				    "\"%s\" is too long (must be < %d)\n",
				    q, IFNAMSIZ);
			}
			if (*p == '\0')
				break;
			q = p + 1;
		}
	}
set:
	mac_lomac_set_single(dest, grade, 0);
	mac_lomac_set_range(dest, grade, 0, grade, 0);
}

static void
mac_lomac_create_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(fragmentlabel);
	dest = SLOT(ipqlabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *datagram, struct label *datagramlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(ipqlabel);
	dest = SLOT(datagramlabel);

	/* Just use the head, since we require them all to match. */
	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_fragment(struct mbuf *datagram, struct label *datagramlabel,
    struct mbuf *fragment, struct label *fragmentlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(datagramlabel);
	dest = SLOT(fragmentlabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_mbuf_from_mbuf(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf,
    struct label *newmbuflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(oldmbuflabel);
	dest = SLOT(newmbuflabel);

	/*
	 * Because the source mbuf may not yet have been "created",
	 * just initialized, we do a conditional copy.  Since we don't
	 * allow mbufs to have ranges, do a KASSERT to make sure that
	 * doesn't happen.
	 */
	KASSERT((source->ml_flags & MAC_LOMAC_FLAG_RANGE) == 0,
	    ("mac_lomac_create_mbuf_from_mbuf: source mbuf has range"));
	mac_lomac_copy(source, dest);
}

static void
mac_lomac_create_mbuf_linklayer(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{
	struct mac_lomac *dest;

	dest = SLOT(mbuflabel);

	mac_lomac_set_single(dest, MAC_LOMAC_TYPE_EQUAL, 0);
}

static void
mac_lomac_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct label *bpflabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(bpflabel);
	dest = SLOT(mbuflabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_mbuf_from_ifnet(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(ifnetlabel);
	dest = SLOT(mbuflabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(oldmbuflabel);
	dest = SLOT(newmbuflabel);

	mac_lomac_copy_single(source, dest);
}

static void
mac_lomac_create_mbuf_netlayer(struct mbuf *oldmbuf, struct label *oldmbuflabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(oldmbuflabel);
	dest = SLOT(newmbuflabel);

	mac_lomac_copy_single(source, dest);
}

static int
mac_lomac_fragment_match(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{
	struct mac_lomac *a, *b;

	a = SLOT(ipqlabel);
	b = SLOT(fragmentlabel);

	return (mac_lomac_equal_single(a, b));
}

static void
mac_lomac_relabel_ifnet(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(ifnetlabel);

	try_relabel(source, dest);
}

static void
mac_lomac_update_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	/* NOOP: we only accept matching labels, so no need to update */
}

/*
 * Labeling event operations: processes.
 */
static void
mac_lomac_create_cred(struct ucred *cred_parent, struct ucred *cred_child)
{
	struct mac_lomac *source, *dest;

	source = SLOT(&cred_parent->cr_label);
	dest = SLOT(&cred_child->cr_label);

	mac_lomac_copy_single(source, dest);
	mac_lomac_copy_range(source, dest);
}

static void
mac_lomac_execve_transition(struct ucred *old, struct ucred *new,
    struct vnode *vp, struct label *vnodelabel,
    struct label *interpvnodelabel, struct image_params *imgp,
    struct label *execlabel)
{
	struct mac_lomac *source, *dest, *obj, *robj;

	source = SLOT(&old->cr_label);
	dest = SLOT(&new->cr_label);
	obj = SLOT(vnodelabel);
	robj = interpvnodelabel != NULL ? SLOT(interpvnodelabel) : obj;

	mac_lomac_copy(source, dest);
	/*
	 * If there's an auxiliary label on the real object, respect it
	 * and assume that this level should be assumed immediately if
	 * a higher level is currently in place.
	 */
	if (robj->ml_flags & MAC_LOMAC_FLAG_AUX &&
	    !mac_lomac_dominate_element(&robj->ml_auxsingle, &dest->ml_single)
	    && mac_lomac_auxsingle_in_range(robj, dest))
		mac_lomac_set_single(dest, robj->ml_auxsingle.mle_type,
		    robj->ml_auxsingle.mle_grade);
	/*
	 * Restructuring to use the execve transitioning mechanism
	 * instead of the normal demotion mechanism here would be
	 * difficult, so just copy the label over and perform standard
	 * demotion.  This is also non-optimal because it will result
	 * in the intermediate label "new" being created and immediately
	 * recycled.
	 */
	if (mac_lomac_enabled && revocation_enabled &&
	    !mac_lomac_dominate_single(obj, source))
		(void)maybe_demote(source, obj, "executing", "file", vp);
}

static int
mac_lomac_execve_will_transition(struct ucred *old, struct vnode *vp,
    struct label *vnodelabel, struct label *interpvnodelabel,
    struct image_params *imgp, struct label *execlabel)
{
	struct mac_lomac *subj, *obj, *robj;

	if (!mac_lomac_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(&old->cr_label);
	obj = SLOT(vnodelabel);
	robj = interpvnodelabel != NULL ? SLOT(interpvnodelabel) : obj;

	return ((robj->ml_flags & MAC_LOMAC_FLAG_AUX &&
	    !mac_lomac_dominate_element(&robj->ml_auxsingle, &subj->ml_single)
	    && mac_lomac_auxsingle_in_range(robj, subj)) ||
	    !mac_lomac_dominate_single(obj, subj));
}

static void
mac_lomac_create_proc0(struct ucred *cred)
{
	struct mac_lomac *dest;

	dest = SLOT(&cred->cr_label);

	mac_lomac_set_single(dest, MAC_LOMAC_TYPE_EQUAL, 0);
	mac_lomac_set_range(dest, MAC_LOMAC_TYPE_LOW, 0, MAC_LOMAC_TYPE_HIGH,
	    0);
}

static void
mac_lomac_create_proc1(struct ucred *cred)
{
	struct mac_lomac *dest;

	dest = SLOT(&cred->cr_label);

	mac_lomac_set_single(dest, MAC_LOMAC_TYPE_HIGH, 0);
	mac_lomac_set_range(dest, MAC_LOMAC_TYPE_LOW, 0, MAC_LOMAC_TYPE_HIGH,
	    0);
}

static void
mac_lomac_relabel_cred(struct ucred *cred, struct label *newlabel)
{
	struct mac_lomac *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(&cred->cr_label);

	try_relabel(source, dest);
}

/*
 * Access control checks.
 */
static int
mac_lomac_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnetlabel)
{
	struct mac_lomac *a, *b;

	if (!mac_lomac_enabled)
		return (0);

	a = SLOT(bpflabel);
	b = SLOT(ifnetlabel);

	if (mac_lomac_equal_single(a, b))
		return (0);
	return (EACCES);
}

static int
mac_lomac_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{
	struct mac_lomac *subj, *new;
	int error;

	subj = SLOT(&cred->cr_label);
	new = SLOT(newlabel);

	/*
	 * If there is a LOMAC label update for the credential, it may
	 * be an update of the single, range, or both.
	 */
	error = lomac_atmostflags(new, MAC_LOMAC_FLAGS_BOTH);
	if (error)
		return (error);

	/*
	 * If the LOMAC label is to be changed, authorize as appropriate.
	 */
	if (new->ml_flags & MAC_LOMAC_FLAGS_BOTH) {
		/*
		 * To change the LOMAC single label on a credential, the
		 * new single label must be in the current range.
		 */
		if (new->ml_flags & MAC_LOMAC_FLAG_SINGLE &&
		    !mac_lomac_single_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the LOMAC range on a credential, the new
		 * range label must be in the current range.
		 */
		if (new->ml_flags & MAC_LOMAC_FLAG_RANGE &&
		    !mac_lomac_range_in_range(new, subj))
			return (EPERM);

		/*
		 * To have EQUAL in any component of the new credential
		 * LOMAC label, the subject must already have EQUAL in
		 * their label.
		 */
		if (mac_lomac_contains_equal(new)) {
			error = mac_lomac_subject_privileged(subj);
			if (error)
				return (error);
		}

		/*
		 * XXXMAC: Additional consistency tests regarding the
		 * single and range of the new label might be performed
		 * here.
		 */
	}

	return (0);
}

static int
mac_lomac_check_cred_visible(struct ucred *u1, struct ucred *u2)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&u1->cr_label);
	obj = SLOT(&u2->cr_label);

	/* XXX: range */
	if (!mac_lomac_dominate_single(obj, subj))
		return (ESRCH);

	return (0);
}

static int
mac_lomac_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{
	struct mac_lomac *subj, *new;
	int error;

	subj = SLOT(&cred->cr_label);
	new = SLOT(newlabel);

	/*
	 * If there is a LOMAC label update for the interface, it may
	 * be an update of the single, range, or both.
	 */
	error = lomac_atmostflags(new, MAC_LOMAC_FLAGS_BOTH);
	if (error)
		return (error);

	/*
	 * Relabling network interfaces requires LOMAC privilege.
	 */
	error = mac_lomac_subject_privileged(subj);
	if (error)
		return (error);

	/*
	 * If the LOMAC label is to be changed, authorize as appropriate.
	 */
	if (new->ml_flags & MAC_LOMAC_FLAGS_BOTH) {
		/*
		 * Rely on the traditional superuser status for the LOMAC
		 * interface relabel requirements.  XXXMAC: This will go
		 * away.
		 */
		error = suser_cred(cred, 0);
		if (error)
			return (EPERM);

		/*
		 * XXXMAC: Additional consistency tests regarding the single
		 * and the range of the new label might be performed here.
		 */
	}

	return (0);
}

static int
mac_lomac_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_lomac *p, *i;

	if (!mac_lomac_enabled)
		return (0);

	p = SLOT(mbuflabel);
	i = SLOT(ifnetlabel);

	return (mac_lomac_single_in_range(p, i) ? 0 : EACCES);
}

static int
mac_lomac_check_kld_load(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (mac_lomac_subject_privileged(subj))
		return (EPERM);

	if (!mac_lomac_high_single(obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_kld_unload(struct ucred *cred)
{
	struct mac_lomac *subj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);

	if (mac_lomac_subject_privileged(subj))
		return (EPERM);

	return (0);
}

static int
mac_lomac_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, unsigned long cmd, void /* caddr_t */ *data)
{

	if(!mac_lomac_enabled)
		return (0);

	/* XXX: This will be implemented soon... */

	return (0);
}

static int
mac_lomac_check_pipe_read(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT((pipelabel));

	if (!mac_lomac_dominate_single(obj, subj))
		return (maybe_demote(subj, obj, "reading", "pipe", NULL));

	return (0);
}

static int
mac_lomac_check_pipe_relabel(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{
	struct mac_lomac *subj, *obj, *new;
	int error;

	new = SLOT(newlabel);
	subj = SLOT(&cred->cr_label);
	obj = SLOT(pipelabel);

	/*
	 * If there is a LOMAC label update for a pipe, it must be a
	 * single update.
	 */
	error = lomac_atmostflags(new, MAC_LOMAC_FLAG_SINGLE);
	if (error)
		return (error);

	/*
	 * To perform a relabel of a pipe (LOMAC label or not), LOMAC must
	 * authorize the relabel.
	 */
	if (!mac_lomac_single_in_range(obj, subj))
		return (EPERM);

	/*
	 * If the LOMAC label is to be changed, authorize as appropriate.
	 */
	if (new->ml_flags & MAC_LOMAC_FLAG_SINGLE) {
		/*
		 * To change the LOMAC label on a pipe, the new pipe label
		 * must be in the subject range.
		 */
		if (!mac_lomac_single_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the LOMAC label on a pipe to be EQUAL, the
		 * subject must have appropriate privilege.
		 */
		if (mac_lomac_contains_equal(new)) {
			error = mac_lomac_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_lomac_check_pipe_write(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT((pipelabel));

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_proc_debug(struct ucred *cred, struct proc *proc)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(&proc->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_lomac_dominate_single(obj, subj))
		return (ESRCH);
	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_proc_sched(struct ucred *cred, struct proc *proc)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(&proc->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_lomac_dominate_single(obj, subj))
		return (ESRCH);
	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_proc_signal(struct ucred *cred, struct proc *proc, int signum)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(&proc->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_lomac_dominate_single(obj, subj))
		return (ESRCH);
	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_socket_deliver(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_lomac *p, *s;

	if (!mac_lomac_enabled)
		return (0);

	p = SLOT(mbuflabel);
	s = SLOT(socketlabel);

	return (mac_lomac_equal_single(p, s) ? 0 : EACCES);
}

static int
mac_lomac_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{
	struct mac_lomac *subj, *obj, *new;
	int error;

	new = SLOT(newlabel);
	subj = SLOT(&cred->cr_label);
	obj = SLOT(socketlabel);

	/*
	 * If there is a LOMAC label update for the socket, it may be
	 * an update of single.
	 */
	error = lomac_atmostflags(new, MAC_LOMAC_FLAG_SINGLE);
	if (error)
		return (error);

	/*
	 * To relabel a socket, the old socket single must be in the subject
	 * range.
	 */
	if (!mac_lomac_single_in_range(obj, subj))
		return (EPERM);

	/*
	 * If the LOMAC label is to be changed, authorize as appropriate.
	 */
	if (new->ml_flags & MAC_LOMAC_FLAG_SINGLE) {
		/*
		 * To relabel a socket, the new socket single must be in
		 * the subject range.
		 */
		if (!mac_lomac_single_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the LOMAC label on the socket to contain EQUAL,
		 * the subject must have appropriate privilege.
		 */
		if (mac_lomac_contains_equal(new)) {
			error = mac_lomac_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_lomac_check_socket_visible(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(socketlabel);

	if (!mac_lomac_dominate_single(obj, subj))
		return (ENOENT);

	return (0);
}

static int
mac_lomac_check_system_swapon(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (mac_lomac_subject_privileged(subj))
		return (EPERM);

	if (!mac_lomac_high_single(obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_system_sysctl(struct ucred *cred, int *name, u_int namelen,
    void *old, size_t *oldlenp, int inkernel, void *new, size_t newlen)
{
	struct mac_lomac *subj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);

	/*
	 * In general, treat sysctl variables as lomac/high, but also
	 * require privilege to change them, since they are a
	 * communications channel between grades.  Exempt MIB
	 * queries from this due to undocmented sysctl magic.
	 * XXXMAC: This probably requires some more review.
	 */
	if (new != NULL) {
		if (namelen > 0 && name[0] == 0)
			return (0);

#ifdef notdef
		if (!mac_lomac_subject_dominate_high(subj))
			return (EACCES);
#endif

		if (mac_lomac_subject_privileged(subj))
			return (EPERM);
	}

	return (0);
}

static int
mac_lomac_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp, struct vattr *vap)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);
	if (obj->ml_flags & MAC_LOMAC_FLAG_AUX &&
	    !mac_lomac_dominate_element(&subj->ml_single, &obj->ml_auxsingle))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot)
{
	struct mac_lomac *subj, *obj;

	/*
	 * Rely on the use of open()-time protections to handle
	 * non-revocation cases.
	 */
	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (prot & VM_PROT_WRITE) {
		if (!mac_lomac_subject_dominate(subj, obj))
			return (EACCES);
	}
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		if (!mac_lomac_dominate_single(obj, subj))
			return (maybe_demote(subj, obj, "mapping", "file", vp));
	}

	return (0);
}

static int
mac_lomac_check_vnode_mprotect(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot)
{
	struct mac_lomac *subj, *obj;

	/*
	 * Rely on the use of open()-time protections to handle
	 * non-revocation cases.
	 */
	if (!mac_lomac_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (prot & VM_PROT_WRITE) {
		if (!mac_lomac_subject_dominate(subj, obj))
			return (EACCES);
	}
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		if (!mac_lomac_dominate_single(obj, subj))
			return (EACCES);
	}

	return (0);
}

static void
mac_lomac_check_vnode_mmap_downgrade(struct ucred *cred, struct vnode *vp,
    struct label *label, /* XXX vm_prot_t */ int *prot)
{
	struct mac_lomac *subj, *obj;

	/*
	 * Rely on the use of open()-time protections to handle
	 * non-revocation cases.
	 */
	if (!mac_lomac_enabled || !revocation_enabled)
		return;

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		*prot &= ~VM_PROT_WRITE;
}

static int
mac_lomac_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, int acc_mode)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	/* XXX privilege override for admin? */
	if (acc_mode & (VWRITE | VAPPEND | VADMIN)) {
		if (!mac_lomac_subject_dominate(subj, obj))
			return (EACCES);
	}

	return (0);
}

static int
mac_lomac_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(&active_cred->cr_label);
	obj = SLOT(label);

	if (!mac_lomac_dominate_single(obj, subj))
		return (maybe_demote(subj, obj, "reading", "file", vp));

	return (0);
}

static int
mac_lomac_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *newlabel)
{
	struct mac_lomac *old, *new, *subj;
	int error;

	old = SLOT(vnodelabel);
	new = SLOT(newlabel);
	subj = SLOT(&cred->cr_label);

	/*
	 * If there is a LOMAC label update for the vnode, it must be a
	 * single label, with an optional explicit auxiliary single.
	 */
	error = lomac_atmostflags(new,
	    MAC_LOMAC_FLAG_SINGLE | MAC_LOMAC_FLAG_AUX);
	if (error)
		return (error);

	/*
	 * To perform a relabel of the vnode (LOMAC label or not), LOMAC must
	 * authorize the relabel.
	 */
	if (!mac_lomac_single_in_range(old, subj))
		return (EPERM);

	/*
	 * If the LOMAC label is to be changed, authorize as appropriate.
	 */
	if (new->ml_flags & MAC_LOMAC_FLAG_SINGLE) {
		/*
		 * To change the LOMAC label on a vnode, the new vnode label
		 * must be in the subject range.
		 */
		if (!mac_lomac_single_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the LOMAC label on the vnode to be EQUAL,
		 * the subject must have appropriate privilege.
		 */
		if (mac_lomac_contains_equal(new)) {
			error = mac_lomac_subject_privileged(subj);
			if (error)
				return (error);
		}
	}
	if (new->ml_flags & MAC_LOMAC_FLAG_AUX) {
		/*
		 * To change the auxiliary LOMAC label on a vnode, the new
		 * vnode label must be in the subject range.
		 */
		if (!mac_lomac_auxsingle_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the auxiliary LOMAC label on the vnode to be
		 * EQUAL, the subject must have appropriate privilege.
		 */
		if (mac_lomac_contains_equal(new)) {
			error = mac_lomac_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_lomac_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label, int samedir,
    struct componentname *cnp)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	if (vp != NULL) {
		obj = SLOT(label);

		if (!mac_lomac_subject_dominate(subj, obj))
			return (EACCES);
	}

	return (0);
}

static int
mac_lomac_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type, struct acl *acl)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, int attrnamespace, const char *name,
    struct uio *uio)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	/* XXX: protect the MAC EA in a special way? */

	return (0);
}

static int
mac_lomac_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, u_long flags)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, mode_t mode)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, uid_t uid, gid_t gid)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct timespec atime, struct timespec mtime)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_lomac_check_vnode_write(struct ucred *active_cred,
    struct ucred *file_cred, struct vnode *vp, struct label *label)
{
	struct mac_lomac *subj, *obj;

	if (!mac_lomac_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(&active_cred->cr_label);
	obj = SLOT(label);

	if (!mac_lomac_subject_dominate(subj, obj))
		return (EACCES);

	return (0);
}

static void
mac_lomac_thread_userret(struct thread *td)
{
	struct proc *p = td->td_proc;
	struct mac_lomac_proc *subj = PSLOT(&p->p_label);
	struct ucred *newcred, *oldcred;
	int dodrop;

	mtx_lock(&subj->mtx);
	if (subj->mac_lomac.ml_flags & MAC_LOMAC_FLAG_UPDATE) {
		dodrop = 0;
		mtx_unlock(&subj->mtx);
		newcred = crget();
		/*
		 * Prevent a lock order reversal in
		 * mac_cred_mmapped_drop_perms; ideally, the other
		 * user of subj->mtx wouldn't be holding Giant.
		 */
		mtx_lock(&Giant);
		PROC_LOCK(p);
		mtx_lock(&subj->mtx);
		/*
		 * Check if we lost the race while allocating the cred.
		 */
		if ((subj->mac_lomac.ml_flags & MAC_LOMAC_FLAG_UPDATE) == 0) {
			crfree(newcred);
			goto out;
		}
		oldcred = p->p_ucred;
		crcopy(newcred, oldcred);
		crhold(newcred);
		mac_lomac_copy(&subj->mac_lomac, SLOT(&newcred->cr_label));
		p->p_ucred = newcred;
		crfree(oldcred);
		dodrop = 1;
	out:
		mtx_unlock(&subj->mtx);
		PROC_UNLOCK(p);
		if (dodrop)
			mac_cred_mmapped_drop_perms(curthread, newcred);
		mtx_unlock(&Giant);
	} else {
		mtx_unlock(&subj->mtx);
	}
}

static struct mac_policy_ops mac_lomac_ops =
{
	.mpo_destroy = mac_lomac_destroy,
	.mpo_init = mac_lomac_init,
	.mpo_init_bpfdesc_label = mac_lomac_init_label,
	.mpo_init_cred_label = mac_lomac_init_label,
	.mpo_init_devfsdirent_label = mac_lomac_init_label,
	.mpo_init_ifnet_label = mac_lomac_init_label,
	.mpo_init_ipq_label = mac_lomac_init_label,
	.mpo_init_mbuf_label = mac_lomac_init_label_waitcheck,
	.mpo_init_mount_label = mac_lomac_init_label,
	.mpo_init_mount_fs_label = mac_lomac_init_label,
	.mpo_init_pipe_label = mac_lomac_init_label,
	.mpo_init_proc_label = mac_lomac_init_proc_label,
	.mpo_init_socket_label = mac_lomac_init_label_waitcheck,
	.mpo_init_socket_peer_label = mac_lomac_init_label_waitcheck,
	.mpo_init_vnode_label = mac_lomac_init_label,
	.mpo_destroy_bpfdesc_label = mac_lomac_destroy_label,
	.mpo_destroy_cred_label = mac_lomac_destroy_label,
	.mpo_destroy_devfsdirent_label = mac_lomac_destroy_label,
	.mpo_destroy_ifnet_label = mac_lomac_destroy_label,
	.mpo_destroy_ipq_label = mac_lomac_destroy_label,
	.mpo_destroy_mbuf_label = mac_lomac_destroy_label,
	.mpo_destroy_mount_label = mac_lomac_destroy_label,
	.mpo_destroy_mount_fs_label = mac_lomac_destroy_label,
	.mpo_destroy_pipe_label = mac_lomac_destroy_label,
	.mpo_destroy_proc_label = mac_lomac_destroy_proc_label,
	.mpo_destroy_socket_label = mac_lomac_destroy_label,
	.mpo_destroy_socket_peer_label = mac_lomac_destroy_label,
	.mpo_destroy_vnode_label = mac_lomac_destroy_label,
	.mpo_copy_pipe_label = mac_lomac_copy_label,
	.mpo_copy_vnode_label = mac_lomac_copy_label,
	.mpo_externalize_cred_label = mac_lomac_externalize_label,
	.mpo_externalize_ifnet_label = mac_lomac_externalize_label,
	.mpo_externalize_pipe_label = mac_lomac_externalize_label,
	.mpo_externalize_socket_label = mac_lomac_externalize_label,
	.mpo_externalize_socket_peer_label = mac_lomac_externalize_label,
	.mpo_externalize_vnode_label = mac_lomac_externalize_label,
	.mpo_internalize_cred_label = mac_lomac_internalize_label,
	.mpo_internalize_ifnet_label = mac_lomac_internalize_label,
	.mpo_internalize_pipe_label = mac_lomac_internalize_label,
	.mpo_internalize_socket_label = mac_lomac_internalize_label,
	.mpo_internalize_vnode_label = mac_lomac_internalize_label,
	.mpo_create_devfs_device = mac_lomac_create_devfs_device,
	.mpo_create_devfs_directory = mac_lomac_create_devfs_directory,
	.mpo_create_devfs_symlink = mac_lomac_create_devfs_symlink,
	.mpo_create_mount = mac_lomac_create_mount,
	.mpo_create_root_mount = mac_lomac_create_root_mount,
	.mpo_relabel_vnode = mac_lomac_relabel_vnode,
	.mpo_update_devfsdirent = mac_lomac_update_devfsdirent,
	.mpo_associate_vnode_devfs = mac_lomac_associate_vnode_devfs,
	.mpo_associate_vnode_extattr = mac_lomac_associate_vnode_extattr,
	.mpo_associate_vnode_singlelabel =
	    mac_lomac_associate_vnode_singlelabel,
	.mpo_create_vnode_extattr = mac_lomac_create_vnode_extattr,
	.mpo_setlabel_vnode_extattr = mac_lomac_setlabel_vnode_extattr,
	.mpo_create_mbuf_from_socket = mac_lomac_create_mbuf_from_socket,
	.mpo_create_pipe = mac_lomac_create_pipe,
	.mpo_create_socket = mac_lomac_create_socket,
	.mpo_create_socket_from_socket = mac_lomac_create_socket_from_socket,
	.mpo_relabel_pipe = mac_lomac_relabel_pipe,
	.mpo_relabel_socket = mac_lomac_relabel_socket,
	.mpo_set_socket_peer_from_mbuf = mac_lomac_set_socket_peer_from_mbuf,
	.mpo_set_socket_peer_from_socket =
	    mac_lomac_set_socket_peer_from_socket,
	.mpo_create_bpfdesc = mac_lomac_create_bpfdesc,
	.mpo_create_datagram_from_ipq = mac_lomac_create_datagram_from_ipq,
	.mpo_create_fragment = mac_lomac_create_fragment,
	.mpo_create_ifnet = mac_lomac_create_ifnet,
	.mpo_create_ipq = mac_lomac_create_ipq,
	.mpo_create_mbuf_from_mbuf = mac_lomac_create_mbuf_from_mbuf,
	.mpo_create_mbuf_linklayer = mac_lomac_create_mbuf_linklayer,
	.mpo_create_mbuf_from_bpfdesc = mac_lomac_create_mbuf_from_bpfdesc,
	.mpo_create_mbuf_from_ifnet = mac_lomac_create_mbuf_from_ifnet,
	.mpo_create_mbuf_multicast_encap =
	    mac_lomac_create_mbuf_multicast_encap,
	.mpo_create_mbuf_netlayer = mac_lomac_create_mbuf_netlayer,
	.mpo_fragment_match = mac_lomac_fragment_match,
	.mpo_relabel_ifnet = mac_lomac_relabel_ifnet,
	.mpo_update_ipq = mac_lomac_update_ipq,
	.mpo_create_cred = mac_lomac_create_cred,
	.mpo_execve_transition = mac_lomac_execve_transition,
	.mpo_execve_will_transition = mac_lomac_execve_will_transition,
	.mpo_create_proc0 = mac_lomac_create_proc0,
	.mpo_create_proc1 = mac_lomac_create_proc1,
	.mpo_relabel_cred = mac_lomac_relabel_cred,
	.mpo_check_bpfdesc_receive = mac_lomac_check_bpfdesc_receive,
	.mpo_check_cred_relabel = mac_lomac_check_cred_relabel,
	.mpo_check_cred_visible = mac_lomac_check_cred_visible,
	.mpo_check_ifnet_relabel = mac_lomac_check_ifnet_relabel,
	.mpo_check_ifnet_transmit = mac_lomac_check_ifnet_transmit,
	.mpo_check_kld_load = mac_lomac_check_kld_load,
	.mpo_check_kld_unload = mac_lomac_check_kld_unload,
	.mpo_check_pipe_ioctl = mac_lomac_check_pipe_ioctl,
	.mpo_check_pipe_read = mac_lomac_check_pipe_read,
	.mpo_check_pipe_relabel = mac_lomac_check_pipe_relabel,
	.mpo_check_pipe_write = mac_lomac_check_pipe_write,
	.mpo_check_proc_debug = mac_lomac_check_proc_debug,
	.mpo_check_proc_sched = mac_lomac_check_proc_sched,
	.mpo_check_proc_signal = mac_lomac_check_proc_signal,
	.mpo_check_socket_deliver = mac_lomac_check_socket_deliver,
	.mpo_check_socket_relabel = mac_lomac_check_socket_relabel,
	.mpo_check_socket_visible = mac_lomac_check_socket_visible,
	.mpo_check_system_swapon = mac_lomac_check_system_swapon,
	.mpo_check_system_sysctl = mac_lomac_check_system_sysctl,
	.mpo_check_vnode_access = mac_lomac_check_vnode_open,
	.mpo_check_vnode_create = mac_lomac_check_vnode_create,
	.mpo_check_vnode_delete = mac_lomac_check_vnode_delete,
	.mpo_check_vnode_deleteacl = mac_lomac_check_vnode_deleteacl,
	.mpo_check_vnode_link = mac_lomac_check_vnode_link,
	.mpo_check_vnode_mmap = mac_lomac_check_vnode_mmap,
	.mpo_check_vnode_mmap_downgrade = mac_lomac_check_vnode_mmap_downgrade,
	.mpo_check_vnode_mprotect = mac_lomac_check_vnode_mprotect,
	.mpo_check_vnode_open = mac_lomac_check_vnode_open,
	.mpo_check_vnode_read = mac_lomac_check_vnode_read,
	.mpo_check_vnode_relabel = mac_lomac_check_vnode_relabel,
	.mpo_check_vnode_rename_from = mac_lomac_check_vnode_rename_from,
	.mpo_check_vnode_rename_to = mac_lomac_check_vnode_rename_to,
	.mpo_check_vnode_revoke = mac_lomac_check_vnode_revoke,
	.mpo_check_vnode_setacl = mac_lomac_check_vnode_setacl,
	.mpo_check_vnode_setextattr = mac_lomac_check_vnode_setextattr,
	.mpo_check_vnode_setflags = mac_lomac_check_vnode_setflags,
	.mpo_check_vnode_setmode = mac_lomac_check_vnode_setmode,
	.mpo_check_vnode_setowner = mac_lomac_check_vnode_setowner,
	.mpo_check_vnode_setutimes = mac_lomac_check_vnode_setutimes,
	.mpo_check_vnode_write = mac_lomac_check_vnode_write,
	.mpo_thread_userret = mac_lomac_thread_userret,
};

MAC_POLICY_SET(&mac_lomac_ops, mac_lomac, "TrustedBSD MAC/LOMAC",
    MPC_LOADTIME_FLAG_NOTLATE, &mac_lomac_slot);
