/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/imgact.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sdt.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/sysctl.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

void
mac_prison_label_free(struct label *label)
{
	if (label == NULL)
		return;

	MAC_POLICY_PERFORM_NOSLEEP(prison_destroy_label, label);
	mac_labelzone_free(label);
}

struct label *
mac_prison_label_alloc(int flag)
{
	struct label *label;
	int error;

	label = mac_labelzone_alloc(flag);
	if (label == NULL)
		return (NULL);

	if (flag & M_WAITOK)
		MAC_POLICY_CHECK(prison_init_label, label, flag);
	else
		MAC_POLICY_CHECK_NOSLEEP(prison_init_label, label, flag);
	if (error) {
		mac_prison_label_free(label);
		return (NULL);
	}
	return (label);
}

/*
 * The caller's expecting us to return with the prison locked if we were
 * successful, since we're also setting pr->pr_label.  On error, it remains
 * unlocked.
 */
int
mac_prison_init(struct prison *pr, int flag)
{
	struct label *prlabel;

	mtx_assert(&pr->pr_mtx, MA_NOTOWNED);
	if ((mac_labeled & MPC_OBJECT_PRISON) == 0) {
		mtx_lock(&pr->pr_mtx);
		pr->pr_label = NULL;
		return (0);
	}

	prlabel = mac_prison_label_alloc(flag);
	if (prlabel == NULL) {
		KASSERT((flag & M_WAITOK) == 0,
		    ("MAC policy prison_init_label failed under M_WAITOK"));
		return (ENOMEM);
	}

	mtx_lock(&pr->pr_mtx);
	pr->pr_label = prlabel;
	return (0);
}

void
mac_prison_destroy(struct prison *pr)
{
	mtx_assert(&pr->pr_mtx, MA_OWNED);
	mac_prison_label_free(pr->pr_label);
	pr->pr_label = NULL;
}

void
mac_prison_copy_label(struct label *src, struct label *dest)
{

	MAC_POLICY_PERFORM_NOSLEEP(prison_copy_label, src, dest);
}

int
mac_prison_externalize_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_POLICY_EXTERNALIZE(prison, label, elements, outbuf, outbuflen);
	return (error);
}

int
mac_prison_internalize_label(struct label *label, char *string)
{
	int error;

	MAC_POLICY_INTERNALIZE(prison, label, string);
	return (error);
}

void
mac_prison_relabel(struct ucred *cred, struct prison *pr,
    struct label *newlabel)
{
	mtx_assert(&pr->pr_mtx, MA_OWNED);
	MAC_POLICY_PERFORM_NOSLEEP(prison_relabel, cred, pr, pr->pr_label,
	    newlabel);
}

int
mac_prison_label_set(struct ucred *cred, struct prison *pr,
    struct label *label)
{
	int error;

	mtx_assert(&pr->pr_mtx, MA_OWNED);

	error = mac_prison_check_relabel(cred, pr, label);
	if (error)
		return (error);

	mac_prison_relabel(cred, pr, label);

	return (0);
}

MAC_CHECK_PROBE_DEFINE4(prison_check_relabel, "struct ucred *",
    "struct prison *", "struct label *", "struct label *");
int
mac_prison_check_relabel(struct ucred *cred, struct prison *pr,
    struct label *newlabel)
{
	int error;

	mtx_assert(&pr->pr_mtx, MA_OWNED);
	MAC_POLICY_CHECK_NOSLEEP(prison_check_relabel, cred, pr,
	    pr->pr_label, newlabel);
	MAC_CHECK_PROBE4(prison_check_relabel, error, cred, pr,
	    pr->pr_label, newlabel);

	return (error);
}

MAC_CHECK_PROBE_DEFINE3(prison_check_attach, "struct ucred *",
    "struct prison *", "struct label *");
int
mac_prison_check_attach(struct ucred *cred, struct prison *pr)
{
	int error;

	MAC_POLICY_CHECK_NOSLEEP(prison_check_attach, cred, pr, pr->pr_label);
	MAC_CHECK_PROBE3(prison_check_attach, error, cred, pr, pr->pr_label);

	return (error);
}

MAC_CHECK_PROBE_DEFINE3(prison_check_create, "struct ucred *",
    "struct vfsoptlist *", "int");
int
mac_prison_check_create(struct ucred *cred, struct vfsoptlist *opts,
    int flags)
{
	int error;

	MAC_POLICY_CHECK_NOSLEEP(prison_check_create, cred, opts, flags);
	MAC_CHECK_PROBE3(prison_check_create, error, cred, opts, flags);

	return (error);
}

MAC_CHECK_PROBE_DEFINE5(prison_check_get, "struct ucred *",
    "struct prison *", "struct label *", "struct vfsoptlist *", "int");
int
mac_prison_check_get(struct ucred *cred, struct prison *pr,
    struct vfsoptlist *opts, int flags)
{
	int error;

	MAC_POLICY_CHECK_NOSLEEP(prison_check_get, cred, pr, pr->pr_label,
	    opts, flags);
	MAC_CHECK_PROBE5(prison_check_get, error, cred, pr, pr->pr_label, opts,
	    flags);

	return (error);
}

MAC_CHECK_PROBE_DEFINE5(prison_check_set, "struct ucred *",
    "struct prison *", "struct label *", "struct vfsoptlist *", "int");
int
mac_prison_check_set(struct ucred *cred, struct prison *pr,
    struct vfsoptlist *opts, int flags)
{
	int error;

	MAC_POLICY_CHECK_NOSLEEP(prison_check_set, cred, pr, pr->pr_label,
	    opts, flags);
	MAC_CHECK_PROBE5(prison_check_set, error, cred, pr, pr->pr_label, opts,
	    flags);

	return (error);
}

MAC_CHECK_PROBE_DEFINE3(prison_check_remove, "struct ucred *",
    "struct prison *", "struct label *");
int
mac_prison_check_remove(struct ucred *cred, struct prison *pr)
{
	int error;

	MAC_POLICY_CHECK_NOSLEEP(prison_check_remove, cred, pr, pr->pr_label);
	MAC_CHECK_PROBE3(prison_check_remove, error, cred, pr, pr->pr_label);

	return (error);
}

void
mac_prison_created(struct ucred *cred, struct prison *pr)
{

	MAC_POLICY_PERFORM(prison_created, cred, pr, pr->pr_label);
}

void
mac_prison_attached(struct ucred *cred, struct prison *pr, struct proc *p)
{

	MAC_POLICY_PERFORM(prison_attached, cred, pr, pr->pr_label, p,
	    p->p_label);
}
