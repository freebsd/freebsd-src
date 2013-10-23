/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

static const char vpsid[] =
    "$Id: vps_priv.c 175 2013-06-13 09:27:34Z klaus $";

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/ioccom.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/syscallsubr.h>
#include <sys/resourcevar.h>
#include <sys/queue.h>
#include <sys/jail.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <net/if.h>
#include <netinet/in.h>

#include <security/mac/mac_framework.h>

#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>

#include "vps_user.h"
#include "vps.h"
#include "vps2.h"
#include "vps_devfsruleset.h"

/* local */
int vps_ip4_check2(struct vps *, struct in_addr *, struct in_addr *, int);
int vps_ip6_check2(struct vps *, struct in6_addr *, u_int8_t, int);

MALLOC_DECLARE(M_VPS_CORE);

#define BIT_SET(set, bit)					\
	do {							\
            set[ bit >> 3 ] |= 1 << (bit - (bit >> 3 << 3 ));   \
        } while (0)

#define BIT_UNSET(set, bit)					\
	do {							\
            set[ bit >> 3 ] &= ~( 1 << (bit - (bit >> 3 << 3 )));\
        } while (0)

#define BIT_ISSET(set, bit)                                     \
    (set[ bit >> 3 ] & 1 << (bit - (bit >> 3 << 3 )) ? 1 : 0)

static struct unrhdr *vps_devfs_unrhdr;

int
vps_priv_check(struct ucred *cred, int priv)
{
        int rv;

	/* Is this syscall/operation implemented ? */
	rv = BIT_ISSET(cred->cr_vps->priv_impl_set, priv);

	if (rv==0) {
		DBGCORE("%s: cred=%p priv=%d NOSYS\n",
			__func__, cred, priv);
		return (ENOSYS);
	}

	/* Is this syscall/operation allowed ? */
        rv = BIT_ISSET(cred->cr_vps->priv_allow_set, priv);

        if (rv==0) {
                DBGCORE("%s: cred=%p priv=%d EPERM\n",
                        __func__, cred, priv);
		return (EPERM);
	}

        return (0);
}

/*
 * Default privileges.
 */
void
vps_priv_setdefault(struct vps *vps, struct vps_param *vps_pr)
{
	u_char *a_set, *i_set;

	a_set = vps->priv_allow_set;
	i_set = vps->priv_impl_set;

	/* Set of allowed operations. */

	memset (a_set, 0, PRIV_SET_SIZE);

	BIT_SET(a_set, PRIV_VFS_MOUNT);
	BIT_SET(a_set, PRIV_VFS_MOUNT_NONUSER);
	BIT_SET(a_set, PRIV_VFS_UNMOUNT);
        BIT_SET(a_set, PRIV_VFS_GENERATION);

        /* Will perform vps destruction if not the vps0. */
        BIT_SET(a_set, PRIV_REBOOT);

        BIT_SET(a_set, PRIV_KTRACE);
        BIT_SET(a_set, PRIV_CRED_SETUID);
        BIT_SET(a_set, PRIV_CRED_SETEUID);
        BIT_SET(a_set, PRIV_CRED_SETGID);
        BIT_SET(a_set, PRIV_CRED_SETEGID);
        BIT_SET(a_set, PRIV_CRED_SETGROUPS);
        BIT_SET(a_set, PRIV_CRED_SETREUID);
        BIT_SET(a_set, PRIV_CRED_SETREGID);
        BIT_SET(a_set, PRIV_CRED_SETRESUID);
        BIT_SET(a_set, PRIV_CRED_SETRESGID);
        BIT_SET(a_set, PRIV_SEEOTHERUIDS);
        BIT_SET(a_set, PRIV_SEEOTHERGIDS);
        BIT_SET(a_set, PRIV_DEBUG_DIFFCRED);
        BIT_SET(a_set, PRIV_DEBUG_SUGID);
        BIT_SET(a_set, PRIV_DEBUG_UNPRIV);
        BIT_SET(a_set, PRIV_PROC_LIMIT);
        BIT_SET(a_set, PRIV_PROC_SETLOGIN);
        BIT_SET(a_set, PRIV_PROC_SETRLIMIT);
        BIT_SET(a_set, PRIV_IPC_READ);
        BIT_SET(a_set, PRIV_IPC_WRITE);
        BIT_SET(a_set, PRIV_IPC_ADMIN);
        BIT_SET(a_set, PRIV_IPC_MSGSIZE);
        BIT_SET(a_set, PRIV_MQ_ADMIN);
        BIT_SET(a_set, PRIV_SCHED_DIFFCRED);
        BIT_SET(a_set, PRIV_SCHED_CPUSET);
        BIT_SET(a_set, PRIV_SIGNAL_DIFFCRED);
        BIT_SET(a_set, PRIV_SIGNAL_SUGID);
        BIT_SET(a_set, PRIV_SYSCTL_WRITEJAIL);
        BIT_SET(a_set, PRIV_VFS_GETQUOTA);
        BIT_SET(a_set, PRIV_VFS_SETQUOTA);
        BIT_SET(a_set, PRIV_VFS_READ);
        BIT_SET(a_set, PRIV_VFS_WRITE);
        BIT_SET(a_set, PRIV_VFS_ADMIN);
        BIT_SET(a_set, PRIV_VFS_EXEC);
        BIT_SET(a_set, PRIV_VFS_LOOKUP);
        BIT_SET(a_set, PRIV_VFS_BLOCKRESERVE);
        BIT_SET(a_set, PRIV_VFS_CHFLAGS_DEV);
        BIT_SET(a_set, PRIV_VFS_CHOWN);
        BIT_SET(a_set, PRIV_VFS_CHROOT);
        BIT_SET(a_set, PRIV_VFS_RETAINSUGID);
        BIT_SET(a_set, PRIV_VFS_FCHROOT);
        BIT_SET(a_set, PRIV_VFS_LINK);
        BIT_SET(a_set, PRIV_VFS_SETGID);
        BIT_SET(a_set, PRIV_VFS_STAT);
        BIT_SET(a_set, PRIV_VFS_STICKYFILE);
        BIT_SET(a_set, PRIV_NETINET_RESERVEDPORT);
        BIT_SET(a_set, PRIV_NETINET_REUSEPORT);
        BIT_SET(a_set, PRIV_NETINET_SETHDROPTS);
        BIT_SET(a_set, PRIV_NETINET_RAW);
        BIT_SET(a_set, PRIV_NETINET_GETCRED);
        BIT_SET(a_set, PRIV_NET_BRIDGE);
        BIT_SET(a_set, PRIV_NET_GRE);
        BIT_SET(a_set, PRIV_NET_BPF);
        BIT_SET(a_set, PRIV_NET_RAW);
        BIT_SET(a_set, PRIV_NET_ROUTE);
        BIT_SET(a_set, PRIV_NET_TAP);
        BIT_SET(a_set, PRIV_NET_SETIFMTU);
        BIT_SET(a_set, PRIV_NET_SETIFFLAGS);
        BIT_SET(a_set, PRIV_NET_SETIFCAP);
        BIT_SET(a_set, PRIV_NET_SETIFNAME);
        BIT_SET(a_set, PRIV_NET_SETIFMETRIC);
        BIT_SET(a_set, PRIV_NET_SETIFPHYS);
        BIT_SET(a_set, PRIV_NET_SETIFMAC);
        BIT_SET(a_set, PRIV_NET_ADDMULTI);
        BIT_SET(a_set, PRIV_NET_DELMULTI);
        BIT_SET(a_set, PRIV_NET_HWIOCTL);
        BIT_SET(a_set, PRIV_NET_SETLLADDR);
        BIT_SET(a_set, PRIV_NET_ADDIFGROUP);
        BIT_SET(a_set, PRIV_NET_DELIFGROUP);
        BIT_SET(a_set, PRIV_NET_IFCREATE);
        BIT_SET(a_set, PRIV_NET_IFDESTROY);
        BIT_SET(a_set, PRIV_NET_ADDIFADDR);
        BIT_SET(a_set, PRIV_NET_DELIFADDR);
        BIT_SET(a_set, PRIV_NET_LAGG);

	/* sysctls are either hidden or virtual */
	BIT_SET(a_set, PRIV_SYSCTL_WRITE);

	/* jail is virtualized */
	BIT_SET(a_set, PRIV_JAIL_ATTACH);
	BIT_SET(a_set, PRIV_JAIL_SET);

	/* Set of available operations (not necessarily allowed). */
	/* Set everything to available by default. */
	memset (i_set, 0xff, PRIV_SET_SIZE);
	/* Remove items in order to have ENOSYS returned. */
	BIT_UNSET(i_set, PRIV_AUDIT_CONTROL);
	BIT_UNSET(i_set, PRIV_AUDIT_FAILSTOP);
	BIT_UNSET(i_set, PRIV_AUDIT_GETAUDIT);
	BIT_UNSET(i_set, PRIV_AUDIT_SETAUDIT);
	BIT_UNSET(i_set, PRIV_AUDIT_SUBMIT);
}

int
vps_devfs_ruleset_create(struct vps *vps)
{
	struct devfs_rule *rule;
	struct devfs_rule *rs;
	size_t size;

	DBGCORE("%s: vps=%p\n", __func__, vps);

	size = sizeof(vps_devfs_ruleset_default);

	rs = malloc(size + sizeof(*rule), M_VPS_CORE, M_WAITOK | M_ZERO);

	memcpy(rs, vps_devfs_ruleset_default, size);

	vps->devfs_ruleset = rs;

	return (0);
}

int
vps_devfs_ruleset_destroy(struct vps *vps)
{

	DBGCORE("%s: vps=%p\n", __func__, vps);

	if (vps->devfs_ruleset == NULL)
		return (0);

	free(vps->devfs_ruleset, M_VPS_CORE);

	vps->devfs_ruleset = NULL;

	return (0);
}

VPSFUNC
static int
vps_devfs_ruleset_apply(struct vps *vps, struct devfs_mount *dm,
    int *ret_rsnum)
{
	struct ucred *save_ucred;
	struct ucred *tmp_ucred;
	struct devfs_rule *rule2;
	struct devfs_rule *rule;
	struct thread *td;
	int ruleset_num;
	int error;

	DBGCORE("%s: vps=%p dm=%p\n", __func__, vps, dm);

	/*
	 * Devfs' ioctl interface checks for PRIV_DEVFS_RULE,
	 * so we can't pass curthread's ucred but have to
	 * set up a privileged one.
	 *
	 * Taking the parent vps make this safe, since the
	 * devfs ruleset is also controlled by the parent vps.
	 *
	 * If the parent vps is not vps0 (no PRIV_DEVFS_RULE),
	 * access will be denied.
	 *
	 * Note: This is way cleaner than the old hack.
	 */
	td = curthread;
	/* Should never get called by a jailed thread. */
	if (jailed(td->td_ucred)) {
		DBGCORE("%s: td=%p is jailed --> EPERM\n",
		    __func__, td);
		return (EPERM);
	}

	rule = malloc(sizeof(*rule), M_TEMP, M_WAITOK);

	save_ucred = td->td_ucred;
	tmp_ucred = crget();
	crcopy(tmp_ucred, save_ucred);

	vps_deref(tmp_ucred->cr_vps, tmp_ucred);
	tmp_ucred->cr_vps = vps->vps_parent;
	vps_ref(tmp_ucred->cr_vps, tmp_ucred);

	prison_free(tmp_ucred->cr_prison);
	tmp_ucred->cr_prison = VPS_VPS(vps->vps_parent, prison0);
	prison_hold(tmp_ucred->cr_prison);

	td->td_ucred = tmp_ucred;
	td->td_vps = vps->vps_parent;

	KASSERT(vps->devfs_ruleset != NULL,
	    ("%s: vps->devfs_ruleset == NULL, vps=%p\n",
	    __func__, vps));

	if (vps_devfs_unrhdr == NULL)
		vps_devfs_unrhdr = new_unrhdr(1, INT_MAX, NULL);
	ruleset_num = alloc_unr(vps_devfs_unrhdr);
	dm->dm_vps_rsnum = ruleset_num;

	rule2 = vps->devfs_ruleset;
	while (rule2->dr_magic != 0) {

		memcpy(rule, rule2, sizeof(*rule));

		rule->dr_id = mkrid(ruleset_num, rid2rn(rule->dr_id));

		DBGCORE("%s: rule=%p dr_id=%d dr_iacts=%d dr_bacts=%d "
		    "dr_pathptrn=[%s]\n",
		    __func__, rule, rule->dr_id, rule->dr_iacts,
		    rule->dr_bacts, rule->dr_pathptrn);

		sx_xlock(&dm->dm_lock);
		error = devfs_rules_ioctl(dm, DEVFSIO_RADD, (caddr_t)rule, td);
		sx_xunlock(&dm->dm_lock);
		if (error != 0) {
			DBGCORE("%s: devfs_rules_ioctl(%p, DEVFSIO_RADD, %p): %d\n",
			    __func__, dm, rule, error);
			goto out;
		}

		rule2++;
	}

	*ret_rsnum = ruleset_num;

  out:
	td->td_vps = vps;
	td->td_ucred = save_ucred;
	crfree(tmp_ucred);
	free(rule, M_TEMP);

	return (error);
}

VPSFUNC
static int
vps_devfs_ruleset_free(struct vps *vps, struct devfs_mount *dm)
{
	struct ucred *save_ucred;
	struct ucred *tmp_ucred;
	struct devfs_rule *rule2;
	struct devfs_rule *rule;
	struct thread *td;
	int ruleset_num;
	devfs_rid rid;
	int error = 0;

	DBGCORE("%s: vps=%p dm=%p\n", __func__, vps, dm);

	/* See comment in vps_devfs_ruleset_apply(). */

	td = curthread;
	if (jailed(td->td_ucred)) {
		DBGCORE("%s: td=%p is jailed --> EPERM\n",
		    __func__, td);
		return (EPERM);
	}

	rule = malloc(sizeof(*rule), M_TEMP, M_WAITOK);

	save_ucred = td->td_ucred;
	tmp_ucred = crget();
	crcopy(tmp_ucred, save_ucred);

	vps_deref(tmp_ucred->cr_vps, tmp_ucred);
	tmp_ucred->cr_vps = vps->vps_parent;
	vps_ref(tmp_ucred->cr_vps, tmp_ucred);

	prison_free(tmp_ucred->cr_prison);
	tmp_ucred->cr_prison = VPS_VPS(vps->vps_parent, prison0);
	prison_hold(tmp_ucred->cr_prison);

	td->td_ucred = tmp_ucred;
	td->td_vps = vps->vps_parent;

	KASSERT(vps->devfs_ruleset != NULL,
	    ("%s: vps->devfs_ruleset == NULL, vps=%p\n",
	    __func__, vps));

	ruleset_num = dm->dm_vps_rsnum;

	rule2 = vps->devfs_ruleset;
	while (rule2->dr_magic != 0) {

		memcpy(rule, rule2, sizeof(*rule));

		rule->dr_id = mkrid(ruleset_num, rid2rn(rule->dr_id));

		DBGCORE("%s: rule=%p dr_id=%d dr_iacts=%d dr_bacts=%d "
		    "dr_pathptrn=[%s]\n",
		    __func__, rule, rule->dr_id, rule->dr_iacts,
		    rule->dr_bacts, rule->dr_pathptrn);

		rid = rid2rn(rule->dr_id);

		sx_xlock(&dm->dm_lock);
		error = devfs_rules_ioctl(dm, DEVFSIO_RDEL, (caddr_t)&rid, td);
		sx_xunlock(&dm->dm_lock);
		if (error != 0) {
			DBGCORE("%s: devfs_rules_ioctl(%p, DEVFSIO_RDEL, %p): %d\n",
			    __func__, dm, &rid, error);
			goto out;
		}

		rule2++;
	}

  out:
	td->td_vps = vps;
	td->td_ucred = save_ucred;
	crfree(tmp_ucred);
	free(rule, M_TEMP);

	return (error);
}

int
vps_devfs_mount_cb(struct devfs_mount *dm, int *rsnum)
{
	struct vps *vps;
	int error;

	vps = curthread->td_vps;

	vps_ref(vps, (void*)0xbeefc0de);
	dm->dm_vps = vps;

	if (vps == vps0)
		return (0);

	/* Load ruleset. */
	error = vps_devfs_ruleset_apply(vps, dm, rsnum);

	return (error);
}

int
vps_devfs_unmount_cb(struct devfs_mount *dm)
{
	struct vps *vps;

	vps = dm->dm_vps;
	vps_deref(vps, (void*)0xbeefc0de);
	dm->dm_vps = NULL;

	if (vps == vps0)
		return (0);

	/* Free ruleset. */
	(void)vps_devfs_ruleset_free(vps, dm);

	return (0);
}

/*
 * Check if a certain device entry should be visible or not to VPS instance
 * referenced by ''dm->dm_vps''.
 * Return code: 0 --> no whiteout
 *              1 --> do whiteout
 */
int
vps_devfs_whiteout_cb(struct devfs_mount *dm, struct cdev_priv *cdp)
{

	if (cdp->cdp_c.si_cred &&
	    cdp->cdp_c.si_cred->cr_vps != dm->dm_vps) {
		/*
		 * This device is a user device (i.e. has user credentials)
		 * AND does not belong in the vps instance this devfs is
		 * mounted in.
		 */
		/*
		DBGCORE("%s: device has ucred and different from "
		    "mountpoint!\n", __func__);
		*/

		return (1);
	}

	return (0);
}

int
vps_canseemount(struct ucred *cred, struct mount *mp)
{
	struct vps *vps;
	char *vpsroot;
	char *mnton;
	int len;
	int error;

	vps = cred->cr_vps;
	error = ENOENT;
	mnton = mp->mnt_stat.f_mntonname;
	vpsroot = vps->_rootpath;

	if (vps->_rootvnode->v_mount == mp) {
		error = 0;
		goto out;
	}

	len = strlen (vpsroot);
	if (vpsroot[len - 1] == '/')
		len -= 1;

	if ((strncmp (vpsroot, mnton, len)) == 0 &&
		(mnton[len] == '\0' || mnton[len] == '/')) {
		error = 0;
		goto out;
	}

out:
	return (error);
}

void
vps_statfs(struct ucred *cred, struct mount *mp, struct statfs *sp)
{
	char buf[MAXPATHLEN];
	struct vps *vps;
	int len;

	vps = cred->cr_vps;

	if (vps == vps0)
		return;

	if (vps->_rootpath[0] == 0)
		return;

	memcpy(buf, sp->f_mntonname, sizeof(buf));
	bzero(sp->f_mntonname, sizeof(sp->f_mntonname));
	len = sizeof(buf) - strlen(vps->_rootpath);
	if (len > sizeof(sp->f_mntonname))
		len = sizeof(sp->f_mntonname);
	memcpy(sp->f_mntonname, buf + strlen(vps->_rootpath), len);
	sp->f_mntonname[sizeof(sp->f_mntonname) - 1] = '\0';

	if (sp->f_mntonname[0] == '\0')
		/* This is the case where for the root fs. */
		strcpy(sp->f_mntonname, "/");

	DBGCORE("%s: vps=%p [%s] --> [%s]\n",
	    __func__, vps, buf, sp->f_mntonname);
}

int
vps_priv_setitem(struct vps *vpsp, struct vps *vps,
    struct vps_arg_item *item)
{
	struct vps_arg_priv *priv;

	if (item->type != VPS_ARG_ITEM_PRIV)
		return (EINVAL);

	priv = &item->u.priv;

	if (item->revoke != 0)
		return (EINVAL);

	if (priv->priv < _PRIV_LOWEST || priv->priv > _PRIV_HIGHEST)
		return (EINVAL);

	switch (priv->value) {
	case VPS_ARG_PRIV_DENY:
		BIT_UNSET(vps->priv_allow_set, priv->priv);
		BIT_SET(vps->priv_impl_set, priv->priv);
		break;
	case VPS_ARG_PRIV_NOSYS:
		BIT_UNSET(vps->priv_impl_set, priv->priv);
		BIT_UNSET(vps->priv_allow_set, priv->priv);
		break;
	case VPS_ARG_PRIV_ALLOW:
		/* Check if parent vps is allowed the priv in question. */
		if (BIT_ISSET(vpsp->priv_allow_set, priv->priv) == 0)
			return (EPERM);
		BIT_SET(vps->priv_allow_set, priv->priv);
		BIT_SET(vps->priv_impl_set, priv->priv);
		break;
	default:
		return (EINVAL);
		break;
	}

	return (0);
}

int
vps_priv_getitemall(struct vps *vpsp, struct vps *vps, caddr_t kdata,
    size_t *kdatalen)
{
	struct vps_arg_item *item;
	int priv;

	for (item = (struct vps_arg_item *)kdata, priv = _PRIV_LOWEST;
	    (caddr_t)(item+1) <= kdata + *kdatalen &&
	    priv <= _PRIV_HIGHEST;
	    item++, priv++) {

		memset(item, 0, sizeof (*item));
		item->type = VPS_ARG_ITEM_PRIV;
		item->u.priv.priv = priv;
		if (BIT_ISSET(vps->priv_impl_set, priv) == 0)
			item->u.priv.value = VPS_ARG_PRIV_NOSYS;
		else if (BIT_ISSET(vps->priv_allow_set, priv) == 0)
			item->u.priv.value = VPS_ARG_PRIV_DENY;
		else
			item->u.priv.value = VPS_ARG_PRIV_ALLOW;
	}

	*kdatalen = (caddr_t)(item) - kdata;

	if (priv != _PRIV_HIGHEST + 1)
		return (ENOSPC);
	else
		return (0);
}

int
vps_ip_setitem(struct vps *vpsp, struct vps *vps,
    struct vps_arg_item *item)
{
	struct vps_arg_ip4 *ip4, *ip4new;
	struct vps_arg_ip6 *ip6, *ip6new;
	int i, j;

	/* XXX locking */

	switch (item->type) {
	case VPS_ARG_ITEM_IP4:
		if (item->revoke != 0) {

			/* Does this entry actually exist ? */
			if (vps_ip4_check2(vps, &item->u.ip4.addr,
			    &item->u.ip4.mask, 1) != 0)
				return (ENOENT);

			ip4new = malloc(sizeof(*ip4) *
			    (vps->vps_ip4_cnt - 1), M_VPS_CORE, M_WAITOK);

			for (ip4 = vps->vps_ip4, i = j = 0;
			    i < vps->vps_ip4_cnt;
			    ip4++, i++)
				if (memcmp(ip4, &item->u.ip4,
				    sizeof(*ip4)) != 0)
					memcpy(&ip4new[j++], ip4,
					    sizeof(*ip4));

			ip4 = vps->vps_ip4;
			/* XXX locking ? */
			vps->vps_ip4 = ip4new;
			vps->vps_ip4_cnt--;
			free(ip4, M_VPS_CORE);

		} else {

			/* Is parent allowed this adress/network ? */
			if (vps_ip4_check2(vpsp, &item->u.ip4.addr,
			    &item->u.ip4.mask, 0) != 0)
				return (EPERM);

			/* Is this adress/network already set ? */
			if (vps_ip4_check2(vps, &item->u.ip4.addr,
			    &item->u.ip4.mask, 1) == 0)
				return (EEXIST);

			ip4new = malloc(sizeof(*ip4) *
			    (vps->vps_ip4_cnt + 1), M_VPS_CORE, M_WAITOK);
			memcpy(ip4new, vps->vps_ip4, vps->vps_ip4_cnt *
			    sizeof(*ip4));
			memcpy(&ip4new[vps->vps_ip4_cnt], &item->u.ip4,
			    sizeof(*ip4));

			ip4 = vps->vps_ip4;
			/* XXX locking ? */
			vps->vps_ip4 = ip4new;
			vps->vps_ip4_cnt++;
			if (ip4 != NULL)
				free(ip4, M_VPS_CORE);
		}
		break;

	case VPS_ARG_ITEM_IP6:
		if (item->revoke != 0) {

			/* Does this entry actually exist ? */
			if (vps_ip6_check2(vps, &item->u.ip6.addr,
			    item->u.ip6.plen, 1) != 0)
				return (ENOENT);

			ip6new = malloc(sizeof(*ip6) *
			    (vps->vps_ip6_cnt - 1), M_VPS_CORE, M_WAITOK);

			for (ip6 = vps->vps_ip6, i = j = 0;
			    i < vps->vps_ip6_cnt;
			    ip6++, i++)
				if (memcmp(ip6, &item->u.ip6,
				    sizeof(*ip6)) != 0)
					memcpy(&ip6new[j++], ip6,
					    sizeof(*ip6));

			ip6 = vps->vps_ip6;
			/* XXX locking ? */
			vps->vps_ip6 = ip6new;
			vps->vps_ip6_cnt--;
			free(ip6, M_VPS_CORE);

		} else {

			/* Is parent allowed this address/network ? */
			if (vps_ip6_check2(vpsp, &item->u.ip6.addr,
			    item->u.ip6.plen, 0) != 0)
				return (EPERM);

			/* Is this address/network already set ? */
			if (vps_ip6_check2(vps, &item->u.ip6.addr,
			    item->u.ip6.plen, 1) == 0)
				return (EEXIST);

			ip6new = malloc(sizeof(*ip6) *
			    (vps->vps_ip6_cnt + 1), M_VPS_CORE, M_WAITOK);
			memcpy(ip6new, vps->vps_ip6, vps->vps_ip6_cnt *
			    sizeof(*ip6));
			memcpy(&ip6new[vps->vps_ip6_cnt], &item->u.ip6,
			    sizeof(*ip6));

			ip6 = vps->vps_ip6;
			/* XXX locking ? */
			vps->vps_ip6 = ip6new;
			vps->vps_ip6_cnt++;
			if (ip6 != NULL)
				free(ip6, M_VPS_CORE);
		}
		break;
	default:
		return (EINVAL);
		break;
	}

	DBGCORE("%s: vps->vps_ip4=%p vps->vps_ip4_cnt=%d\n",
		__func__, vps->vps_ip4, vps->vps_ip4_cnt);
	DBGCORE("%s: vps->vps_ip6=%p vps->vps_ip6_cnt=%d\n",
		__func__, vps->vps_ip6, vps->vps_ip6_cnt);

	return (0);
}

int
vps_ip_getitemall(struct vps *vpsp, struct vps *vps, caddr_t kdata,
    size_t *kdatalen)
{
	struct vps_arg_item *item;
	struct vps_arg_ip4 *ip4;
	struct vps_arg_ip6 *ip6;
	caddr_t kpos;

	for (item = (struct vps_arg_item *)kdata, ip4 = vps->vps_ip4;
	     ip4 != NULL &&
	     	(caddr_t)(item+1) <= kdata + *kdatalen &&
	     	(ip4 - vps->vps_ip4) < vps->vps_ip4_cnt;
	     item++, ip4++) {

		memset(item, 0, sizeof (*item));
		item->type = VPS_ARG_ITEM_IP4;
		memcpy(&item->u.ip4, ip4, sizeof (*ip4));
	}
	kpos = (caddr_t)(item);
	for (item = (struct vps_arg_item *)kpos, ip6 = vps->vps_ip6;
	     ip6 != NULL &&
		(caddr_t)(item+1) <= kdata + *kdatalen &&
		(ip6 - vps->vps_ip6) < vps->vps_ip6_cnt;
	     item++, ip6++) {

		memset(item, 0, sizeof (*item));
		item->type = VPS_ARG_ITEM_IP6;
		memcpy(&item->u.ip6, ip6, sizeof (*ip6));
	}

	*kdatalen = (caddr_t)(item) - kdata;

	if (*kdatalen / sizeof (*item) != vps->vps_ip4_cnt +
	    vps->vps_ip6_cnt)
		return (ENOSPC);
	else
		return (0);
}

/*
 * XXX
 * For now do a simple linear lookup - of course later this has to
 * be replaced by a tree or something else.
 */

int
vps_ip4_check(struct vps *vps, struct in_addr *addr, struct in_addr *mask)
{
	int error;

	/* XXX lock */
	error = vps_ip4_check2(vps, addr, mask, 0);
	/* XXX unlock */

	return (error);
}

int
vps_ip4_check2(struct vps *vps, struct in_addr *addr, struct in_addr *mask,
    int exact)
{
	struct vps_arg_ip4 *ip4;

	if (vps->vps_ip4 == NULL)
		return (EPERM);

	/* Look for exact matches (same netmask) */
	for (ip4 = vps->vps_ip4; (ip4 - vps->vps_ip4) < vps->vps_ip4_cnt;
	    ip4++)
		if (ip4->addr.s_addr == addr->s_addr && ip4->mask.s_addr ==
		    mask->s_addr)
			return (0);

	if (exact != 0)
		return (EPERM);

	/* It is not necessary to find the closest match,
	   any match will do. */
	for (ip4 = vps->vps_ip4; (ip4 - vps->vps_ip4) < vps->vps_ip4_cnt;
	    ip4++)
		if ((addr->s_addr & ip4->mask.s_addr) == ip4->addr.s_addr)
			return (0);

	return (EPERM);
}

int
vps_ip6_check(struct vps *vps, struct in6_addr *addr, u_int8_t plen)
{
	int error;

	/* XXX lock */
	error = vps_ip6_check2(vps, addr, plen, 0);
	/* XXX unlock */

	return (error);
}

int
vps_ip6_check2(struct vps *vps, struct in6_addr *addr, u_int8_t plen,
    int exact)
{
	struct vps_arg_ip6 *ip6;
	struct in6_addr net;

	if (vps->vps_ip6 == NULL)
		return (EPERM);

	/* Look for exact matches (same prefixlen) */
	for (ip6 = vps->vps_ip6; (ip6 - vps->vps_ip6) < vps->vps_ip6_cnt;
	    ip6++)
		if ((memcmp(&ip6->addr, addr, sizeof (*addr)) == 0) &&
		    (ip6->plen == plen))
			return (0);

	if (exact != 0)
		return (EPERM);

	/* It is not necessary to find the closest match, any will do. */
	for (ip6 = vps->vps_ip6; (ip6 - vps->vps_ip6) < vps->vps_ip6_cnt;
	    ip6++) {

		plen = ip6->plen;

		if (plen == 0) {
			net.s6_addr32[0] = 0;
			net.s6_addr32[1] = 0;
			net.s6_addr32[2] = 0;
			net.s6_addr32[3] = 0;
		} else
		if (plen <= 32) {
			net.s6_addr32[0] = htonl(ntohl(addr->s6_addr32[0]) &
			    (0xffffffff << (32-plen)));
			net.s6_addr32[1] = 0;
			net.s6_addr32[2] = 0;
			net.s6_addr32[3] = 0;
		} else
		if (plen <= 64) {
			net.s6_addr32[0] = addr->s6_addr32[0];
			net.s6_addr32[1] = htonl(ntohl(addr->s6_addr32[1]) &
			    (0xffffffff << (32-plen-32)));
			net.s6_addr32[2] = 0;
			net.s6_addr32[3] = 0;
		} else
		if (plen <= 96) {
			net.s6_addr32[0] = addr->s6_addr32[0];
			net.s6_addr32[1] = addr->s6_addr32[1];
			net.s6_addr32[2] = htonl(ntohl(addr->s6_addr32[2]) &
			    (0xffffffff << (32-plen-64)));
			net.s6_addr32[3] = 0;
		} else
		{
			net.s6_addr32[0] = addr->s6_addr32[0];
			net.s6_addr32[1] = addr->s6_addr32[1];
			net.s6_addr32[2] = addr->s6_addr32[2];
			net.s6_addr32[3] = htonl(ntohl(addr->s6_addr32[3]) &
			    (0xffffffff << (32-plen-96)));
		}

		/*
		printf("%s: memcmp %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x "
		    "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x (%lu)\n",
		    __func__,
		    htons(net.s6_addr16[0]), htons(net.s6_addr16[1]),
		    htons(net.s6_addr16[2]), htons(net.s6_addr16[3]),
		    htons(net.s6_addr16[4]), htons(net.s6_addr16[5]),
		    htons(net.s6_addr16[6]), htons(net.s6_addr16[7]),
		    htons(ip6->addr.s6_addr16[0]),
		    htons(ip6->addr.s6_addr16[1]),
		    htons(ip6->addr.s6_addr16[2]),
		    htons(ip6->addr.s6_addr16[3]),
		    htons(ip6->addr.s6_addr16[4]),
		    htons(ip6->addr.s6_addr16[5]),
		    htons(ip6->addr.s6_addr16[6]),
		    htons(ip6->addr.s6_addr16[7]),
		    sizeof(*addr));
		*/

		if (memcmp(&net, &ip6->addr, sizeof(*addr)) == 0)
			return (0);
	}

	return (EPERM);
}

/* EOF */
