/*
 * linux/fs/nfsd/auth.c
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcauth.h>
#include <linux/nfsd/nfsd.h>

#define	CAP_NFSD_MASK (CAP_FS_MASK|CAP_TO_MASK(CAP_SYS_RESOURCE))
void
nfsd_setuser(struct svc_rqst *rqstp, struct svc_export *exp)
{
	struct svc_cred	*cred = &rqstp->rq_cred;
	int		i;

	if (rqstp->rq_userset)
		return;

	if (exp->ex_flags & NFSEXP_ALLSQUASH) {
		cred->cr_uid = exp->ex_anon_uid;
		cred->cr_gid = exp->ex_anon_gid;
		cred->cr_groups[0] = NOGROUP;
	} else if (exp->ex_flags & NFSEXP_ROOTSQUASH) {
		if (!cred->cr_uid)
			cred->cr_uid = exp->ex_anon_uid;
		if (!cred->cr_gid)
			cred->cr_gid = exp->ex_anon_gid;
		for (i = 0; i < NGROUPS; i++)
			if (!cred->cr_groups[i])
				cred->cr_groups[i] = exp->ex_anon_gid;
	}

	if (cred->cr_uid != (uid_t) -1)
		current->fsuid = cred->cr_uid;
	else
		current->fsuid = exp->ex_anon_uid;
	if (cred->cr_gid != (gid_t) -1)
		current->fsgid = cred->cr_gid;
	else
		current->fsgid = exp->ex_anon_gid;
	for (i = 0; i < NGROUPS; i++) {
		gid_t group = cred->cr_groups[i];
		if (group == (gid_t) NOGROUP)
			break;
		current->groups[i] = group;
	}
	current->ngroups = i;

	if ((cred->cr_uid)) {
		cap_t(current->cap_effective) &= ~CAP_NFSD_MASK;
	} else {
		cap_t(current->cap_effective) |= (CAP_NFSD_MASK &
						  current->cap_permitted);
	}

	rqstp->rq_userset = 1;
}
