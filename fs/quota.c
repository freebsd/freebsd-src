/*
 * Quota code necessary even when VFS quota support is not compiled
 * into the kernel.  The interesting stuff is over in dquot.c, here
 * we have symbols for initial quotactl(2) handling, the sysctl(2)
 * variables, etc - things needed even when quota support disabled.
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/quotaops.h>
#include <linux/quotacompat.h>

struct dqstats dqstats;

/* Check validity of quotactl */
static int check_quotactl_valid(struct super_block *sb, int type, int cmd, qid_t id)
{
	if (type >= MAXQUOTAS)
		return -EINVAL;
	if (!sb && cmd != Q_SYNC)
		return -ENODEV;
	/* Is operation supported? */
	if (sb && !sb->s_qcop)
		return -ENOSYS;

	switch (cmd) {
		case Q_GETFMT:
			break;
		case Q_QUOTAON:
			if (!sb->s_qcop->quota_on)
				return -ENOSYS;
			break;
		case Q_QUOTAOFF:
			if (!sb->s_qcop->quota_off)
				return -ENOSYS;
			break;
		case Q_SETINFO:
			if (!sb->s_qcop->set_info)
				return -ENOSYS;
			break;
		case Q_GETINFO:
			if (!sb->s_qcop->get_info)
				return -ENOSYS;
			break;
		case Q_SETQUOTA:
			if (!sb->s_qcop->set_dqblk)
				return -ENOSYS;
			break;
		case Q_GETQUOTA:
			if (!sb->s_qcop->get_dqblk)
				return -ENOSYS;
			break;
		case Q_SYNC:
			if (sb && !sb->s_qcop->quota_sync)
				return -ENOSYS;
			break;
		case Q_XQUOTAON:
		case Q_XQUOTAOFF:
		case Q_XQUOTARM:
			if (!sb->s_qcop->set_xstate)
				return -ENOSYS;
			break;
		case Q_XGETQSTAT:
			if (!sb->s_qcop->get_xstate)
				return -ENOSYS;
			break;
		case Q_XSETQLIM:
			if (!sb->s_qcop->set_xquota)
				return -ENOSYS;
			break;
		case Q_XGETQUOTA:
			if (!sb->s_qcop->get_xquota)
				return -ENOSYS;
			break;
		default:
			return -EINVAL;
	}

	/* Is quota turned on for commands which need it? */
	switch (cmd) {
		case Q_GETFMT:
		case Q_GETINFO:
		case Q_QUOTAOFF:
		case Q_SETINFO:
		case Q_SETQUOTA:
		case Q_GETQUOTA:
			if (!sb_has_quota_enabled(sb, type))
				return -ESRCH;
	}
	/* Check privileges */
	if (cmd == Q_GETQUOTA || cmd == Q_XGETQUOTA) {
		if (((type == USRQUOTA && current->euid != id) ||
		     (type == GRPQUOTA && !in_egroup_p(id))) &&
		    !capable(CAP_SYS_ADMIN))
			return -EPERM;
	}
	else if (cmd != Q_GETFMT && cmd != Q_SYNC && cmd != Q_GETINFO && cmd != Q_XGETQSTAT)
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
	return 0;
}

/* Resolve device pathname to superblock */
static struct super_block *resolve_dev(const char *path)
{
	int ret;
	mode_t mode;
	struct nameidata nd;
	kdev_t dev;
	struct super_block *sb;

	ret = user_path_walk(path, &nd);
	if (ret)
		goto out;

	dev = nd.dentry->d_inode->i_rdev;
	mode = nd.dentry->d_inode->i_mode;
	path_release(&nd);

	ret = -ENOTBLK;
	if (!S_ISBLK(mode))
		goto out;
	ret = -ENODEV;
	sb = get_super(dev);
	if (!sb)
		goto out;
	return sb;
out:
	return ERR_PTR(ret);
}

/* Copy parameters and call proper function */
static int do_quotactl(struct super_block *sb, int type, int cmd, qid_t id, caddr_t addr)
{
	int ret;

	switch (cmd) {
		case Q_QUOTAON: {
			char *pathname;

			if (IS_ERR(pathname = getname(addr)))
				return PTR_ERR(pathname);
			ret = sb->s_qcop->quota_on(sb, type, id, pathname);
			putname(pathname);
			return ret;
		}
		case Q_QUOTAOFF:
			return sb->s_qcop->quota_off(sb, type);

		case Q_GETFMT: {
			__u32 fmt;

			fmt = sb_dqopt(sb)->info[type].dqi_format->qf_fmt_id;
			if (copy_to_user(addr, &fmt, sizeof(fmt)))
				return -EFAULT;
			return 0;
		}
		case Q_GETINFO: {
			struct if_dqinfo info;

			if ((ret = sb->s_qcop->get_info(sb, type, &info)))
				return ret;
			if (copy_to_user(addr, &info, sizeof(info)))
				return -EFAULT;
			return 0;
		}
		case Q_SETINFO: {
			struct if_dqinfo info;

			if (copy_from_user(&info, addr, sizeof(info)))
				return -EFAULT;
			return sb->s_qcop->set_info(sb, type, &info);
		}
		case Q_GETQUOTA: {
			struct if_dqblk idq;

			if ((ret = sb->s_qcop->get_dqblk(sb, type, id, &idq)))
				return ret;
			if (copy_to_user(addr, &idq, sizeof(idq)))
				return -EFAULT;
			return 0;
		}
		case Q_SETQUOTA: {
			struct if_dqblk idq;

			if (copy_from_user(&idq, addr, sizeof(idq)))
				return -EFAULT;
			return sb->s_qcop->set_dqblk(sb, type, id, &idq);
		}
		case Q_SYNC:
			if (sb)
				return sb->s_qcop->quota_sync(sb, type);
			sync_dquots_dev(NODEV, type);
			return 0;
		case Q_XQUOTAON:
		case Q_XQUOTAOFF:
		case Q_XQUOTARM: {
			__u32 flags;

			if (copy_from_user(&flags, addr, sizeof(flags)))
				return -EFAULT;
			return sb->s_qcop->set_xstate(sb, flags, cmd);
		}
		case Q_XGETQSTAT: {
			struct fs_quota_stat fqs;
		
			if ((ret = sb->s_qcop->get_xstate(sb, &fqs)))
				return ret;
			if (copy_to_user(addr, &fqs, sizeof(fqs)))
				return -EFAULT;
			return 0;
		}
		case Q_XSETQLIM: {
			struct fs_disk_quota fdq;

			if (copy_from_user(&fdq, addr, sizeof(fdq)))
				return -EFAULT;
		       return sb->s_qcop->set_xquota(sb, type, id, &fdq);
		}
		case Q_XGETQUOTA: {
			struct fs_disk_quota fdq;

			if ((ret = sb->s_qcop->get_xquota(sb, type, id, &fdq)))
				return ret;
			if (copy_to_user(addr, &fdq, sizeof(fdq)))
				return -EFAULT;
			return 0;
		}
		/* We never reach here unless validity check is broken */
		default:
			BUG();
	}
	return 0;
}

static int check_compat_quotactl_valid(struct super_block *sb, int type, int cmd, qid_t id)
{
	if (type >= MAXQUOTAS)
		return -EINVAL;
	/* Is operation supported? */
	/* sb==NULL for GETSTATS calls */
	if (sb && !sb->s_qcop)
		return -ENOSYS;

	switch (cmd) {
		case Q_COMP_QUOTAON:
			if (!sb->s_qcop->quota_on)
				return -ENOSYS;
			break;
		case Q_COMP_QUOTAOFF:
			if (!sb->s_qcop->quota_off)
				return -ENOSYS;
			break;
		case Q_COMP_SYNC:
			if (sb && !sb->s_qcop->quota_sync)
				return -ENOSYS;
			break;
		case Q_V1_SETQLIM:
		case Q_V1_SETUSE:
		case Q_V1_SETQUOTA:
			if (!sb->s_qcop->set_dqblk)
				return -ENOSYS;
			break;
		case Q_V1_GETQUOTA:
			if (!sb->s_qcop->get_dqblk)
				return -ENOSYS;
			break;
		case Q_V1_RSQUASH:
			if (!sb->s_qcop->set_info)
				return -ENOSYS;
			break;
		case Q_V1_GETSTATS:
			return 0;	/* GETSTATS need no other checks */
		default:
			return -EINVAL;
	}

	/* Is quota turned on for commands which need it? */
	switch (cmd) {
		case Q_V2_SETFLAGS:
		case Q_V2_SETGRACE:
		case Q_V2_SETINFO:
		case Q_V2_GETINFO:
		case Q_COMP_QUOTAOFF:
		case Q_V1_RSQUASH:
		case Q_V1_SETQUOTA:
		case Q_V1_SETQLIM:
		case Q_V1_SETUSE:
		case Q_V2_SETQUOTA:
		/* Q_V2_SETQLIM: collision with Q_V1_SETQLIM */
		case Q_V2_SETUSE:
		case Q_V1_GETQUOTA:
		case Q_V2_GETQUOTA:
			if (!sb_has_quota_enabled(sb, type))
				return -ESRCH;
	}
	if (cmd != Q_COMP_QUOTAON &&
	    cmd != Q_COMP_QUOTAOFF &&
	    cmd != Q_COMP_SYNC &&
	    sb_dqopt(sb)->info[type].dqi_format->qf_fmt_id != QFMT_VFS_OLD)
		return -ESRCH;

	/* Check privileges */
	if (cmd == Q_V1_GETQUOTA || cmd == Q_V2_GETQUOTA) {
		if (((type == USRQUOTA && current->euid != id) ||
		     (type == GRPQUOTA && !in_egroup_p(id))) &&
		    !capable(CAP_SYS_ADMIN))
			return -EPERM;
	}
	else if (cmd != Q_V1_GETSTATS && cmd != Q_V2_GETSTATS && cmd != Q_V2_GETINFO && cmd != Q_COMP_SYNC)
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
	return 0;
}

static int v1_set_rsquash(struct super_block *sb, int type, int flag)
{
	struct if_dqinfo info;

	info.dqi_valid = IIF_FLAGS;
	info.dqi_flags = flag ? V1_DQF_RSQUASH : 0;
	return sb->s_qcop->set_info(sb, type, &info);
}

static int v1_get_dqblk(struct super_block *sb, int type, qid_t id, struct v1c_mem_dqblk *mdq)
{
	struct if_dqblk idq;
	int ret;

	if ((ret = sb->s_qcop->get_dqblk(sb, type, id, &idq)) < 0)
		return ret;
	mdq->dqb_ihardlimit = idq.dqb_ihardlimit;
	mdq->dqb_isoftlimit = idq.dqb_isoftlimit;
	mdq->dqb_curinodes = idq.dqb_curinodes;
	mdq->dqb_bhardlimit = idq.dqb_bhardlimit;
	mdq->dqb_bsoftlimit = idq.dqb_bsoftlimit;
	mdq->dqb_curblocks = toqb(idq.dqb_curspace);
	mdq->dqb_itime = idq.dqb_itime;
	mdq->dqb_btime = idq.dqb_btime;
	if (id == 0) {	/* Times for id 0 are in fact grace times */
		struct if_dqinfo info;

		if ((ret = sb->s_qcop->get_info(sb, type, &info)) < 0)
			return ret;
		mdq->dqb_btime = info.dqi_bgrace;
		mdq->dqb_itime = info.dqi_igrace;
	}
	return 0;
}

static int v1_set_dqblk(struct super_block *sb, int type, int cmd, qid_t id, struct v1c_mem_dqblk *mdq)
{
	struct if_dqblk idq;
	int ret;

	idq.dqb_valid = 0;
	if (cmd == Q_V1_SETQUOTA || cmd == Q_V1_SETQLIM) {
		idq.dqb_ihardlimit = mdq->dqb_ihardlimit;
		idq.dqb_isoftlimit = mdq->dqb_isoftlimit;
		idq.dqb_bhardlimit = mdq->dqb_bhardlimit;
		idq.dqb_bsoftlimit = mdq->dqb_bsoftlimit;
		idq.dqb_valid |= QIF_LIMITS;
	}
	if (cmd == Q_V1_SETQUOTA || cmd == Q_V1_SETUSE) {
		idq.dqb_curinodes = mdq->dqb_curinodes;
		idq.dqb_curspace = ((qsize_t)mdq->dqb_curblocks) << QUOTABLOCK_BITS;
		idq.dqb_valid |= QIF_USAGE;
	}
	ret = sb->s_qcop->set_dqblk(sb, type, id, &idq);
	if (!ret && id == 0 && cmd == Q_V1_SETQUOTA) {	/* Times for id 0 are in fact grace times */
		struct if_dqinfo info;

		info.dqi_bgrace = mdq->dqb_btime;
		info.dqi_igrace = mdq->dqb_itime;
		info.dqi_valid = IIF_BGRACE | IIF_IGRACE;
		ret = sb->s_qcop->set_info(sb, type, &info);
	}
	return ret;
}

static void v1_get_stats(struct v1c_dqstats *dst)
{
	memcpy(dst, &dqstats, sizeof(dqstats));
}

/* Handle requests to old interface */
static int do_compat_quotactl(struct super_block *sb, int type, int cmd, qid_t id, caddr_t addr)
{
	int ret;

	switch (cmd) {
		case Q_COMP_QUOTAON: {
			char *pathname;

			if (IS_ERR(pathname = getname(addr)))
				return PTR_ERR(pathname);
			ret = sb->s_qcop->quota_on(sb, type, QFMT_VFS_OLD, pathname);
			putname(pathname);
			return ret;
		}
		case Q_COMP_QUOTAOFF:
			return sb->s_qcop->quota_off(sb, type);
		case Q_COMP_SYNC:
			if (sb)
				return sb->s_qcop->quota_sync(sb, type);
			sync_dquots_dev(NODEV, type);
			return 0;
		case Q_V1_RSQUASH: {
			int flag;

			if (copy_from_user(&flag, addr, sizeof(flag)))
				return -EFAULT;
			return v1_set_rsquash(sb, type, flag);
		}
		case Q_V1_GETQUOTA: {
			struct v1c_mem_dqblk mdq;

			if ((ret = v1_get_dqblk(sb, type, id, &mdq)))
				return ret;
			if (copy_to_user(addr, &mdq, sizeof(mdq)))
				return -EFAULT;
			return 0;
		}
		case Q_V1_SETQLIM:
		case Q_V1_SETUSE:
		case Q_V1_SETQUOTA: {
			struct v1c_mem_dqblk mdq;

			if (copy_from_user(&mdq, addr, sizeof(mdq)))
				return -EFAULT;
			return v1_set_dqblk(sb, type, cmd, id, &mdq);
		}
		case Q_V1_GETSTATS: {
			struct v1c_dqstats dst;

			v1_get_stats(&dst);
			if (copy_to_user(addr, &dst, sizeof(dst)))
				return -EFAULT;
			return 0;
		}
	}
	BUG();
	return 0;
}

/* Macros for short-circuiting the compatibility tests */
#define NEW_COMMAND(c) ((c) & (0x80 << 16))
#define XQM_COMMAND(c) (((c) & ('X' << 8)) == ('X' << 8))

/*
 * This is the system call interface. This communicates with
 * the user-level programs. Currently this only supports diskquota
 * calls. Maybe we need to add the process quotas etc. in the future,
 * but we probably should use rlimits for that.
 */
asmlinkage long sys_quotactl(unsigned int cmd, const char *special, qid_t id, caddr_t addr)
{
	uint cmds, type;
	struct super_block *sb = NULL;
	int ret = -EINVAL;

	lock_kernel();
	cmds = cmd >> SUBCMDSHIFT;
	type = cmd & SUBCMDMASK;

	if (cmds != Q_V1_GETSTATS && cmds != Q_V2_GETSTATS && IS_ERR(sb = resolve_dev(special))) {
		ret = PTR_ERR(sb);
		sb = NULL;
		goto out;
	}
	if (!NEW_COMMAND(cmds) && !XQM_COMMAND(cmds)) {
		if ((ret = check_compat_quotactl_valid(sb, type, cmds, id)) < 0)
			goto out;
		ret = do_compat_quotactl(sb, type, cmds, id, addr);
		goto out;
	}
	if ((ret = check_quotactl_valid(sb, type, cmds, id)) < 0)
		goto out;
	ret = do_quotactl(sb, type, cmds, id, addr);
out:
	if (sb)
		drop_super(sb);
	unlock_kernel();
	return ret;
}
