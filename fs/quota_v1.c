#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/quota.h>
#include <linux/dqblk_v1.h>
#include <linux/quotaio_v1.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/byteorder.h>

static void v1_disk2mem_dqblk(struct mem_dqblk *m, struct v1_disk_dqblk *d)
{
	m->dqb_ihardlimit = d->dqb_ihardlimit;
	m->dqb_isoftlimit = d->dqb_isoftlimit;
	m->dqb_curinodes = d->dqb_curinodes;
	m->dqb_bhardlimit = d->dqb_bhardlimit;
	m->dqb_bsoftlimit = d->dqb_bsoftlimit;
	m->dqb_curspace = ((qsize_t)d->dqb_curblocks) << QUOTABLOCK_BITS;
	m->dqb_itime = d->dqb_itime;
	m->dqb_btime = d->dqb_btime;
}

static void v1_mem2disk_dqblk(struct v1_disk_dqblk *d, struct mem_dqblk *m)
{
	d->dqb_ihardlimit = m->dqb_ihardlimit;
	d->dqb_isoftlimit = m->dqb_isoftlimit;
	d->dqb_curinodes = m->dqb_curinodes;
	d->dqb_bhardlimit = m->dqb_bhardlimit;
	d->dqb_bsoftlimit = m->dqb_bsoftlimit;
	d->dqb_curblocks = toqb(m->dqb_curspace);
	d->dqb_itime = m->dqb_itime;
	d->dqb_btime = m->dqb_btime;
}

static int v1_read_dqblk(struct dquot *dquot)
{
	int type = dquot->dq_type;
	struct file *filp;
	mm_segment_t fs;
	loff_t offset;
	struct v1_disk_dqblk dqblk;

	filp = sb_dqopt(dquot->dq_sb)->files[type];
	if (filp == (struct file *)NULL)
		return -EINVAL;

	memset(&dqblk, 0, sizeof(dqblk));	/* Initialize buffer in case file is too short */
	/* Now we are sure filp is valid */
	offset = v1_dqoff(dquot->dq_id);
	fs = get_fs();
	set_fs(KERNEL_DS);
	filp->f_op->read(filp, (char *)&dqblk, sizeof(struct v1_disk_dqblk), &offset);
	set_fs(fs);

	v1_disk2mem_dqblk(&dquot->dq_dqb, &dqblk);
	if (dquot->dq_dqb.dqb_bhardlimit == 0 && dquot->dq_dqb.dqb_bsoftlimit == 0 &&
	    dquot->dq_dqb.dqb_ihardlimit == 0 && dquot->dq_dqb.dqb_isoftlimit == 0)
		dquot->dq_flags |= DQ_FAKE;
	dqstats.reads++;
	return 0;
}

static int v1_commit_dqblk(struct dquot *dquot)
{
	short type = dquot->dq_type;
	struct file *filp;
	mm_segment_t fs;
	loff_t offset;
	ssize_t ret;
	struct v1_disk_dqblk dqblk;

	filp = sb_dqopt(dquot->dq_sb)->files[type];
	offset = v1_dqoff(dquot->dq_id);
	fs = get_fs();
	set_fs(KERNEL_DS);

	/*
	 * Note: clear the DQ_MOD flag unconditionally,
	 * so we don't loop forever on failure.
	 */
	v1_mem2disk_dqblk(&dqblk, &dquot->dq_dqb);
	dquot->dq_flags &= ~DQ_MOD;
	if (dquot->dq_id == 0) {
		dqblk.dqb_btime = sb_dqopt(dquot->dq_sb)->info[type].dqi_bgrace;
		dqblk.dqb_itime = sb_dqopt(dquot->dq_sb)->info[type].dqi_igrace;
	}
	ret = 0;
	if (filp)
		ret = filp->f_op->write(filp, (char *)&dqblk,
					sizeof(struct v1_disk_dqblk), &offset);
	if (ret != sizeof(struct v1_disk_dqblk)) {
		printk(KERN_WARNING "VFS: dquota write failed on dev %s\n",
			kdevname(dquot->dq_dev));
		if (ret >= 0)
			ret = -EIO;
		goto out;
	}
	ret = 0;

out:
	set_fs(fs);
	dqstats.writes++;
	return ret;
}

/* Magics of new quota format */
#define V2_INITQMAGICS {\
	0xd9c01f11,     /* USRQUOTA */\
	0xd9c01927      /* GRPQUOTA */\
}

/* Header of new quota format */
struct v2_disk_dqheader {
	__u32 dqh_magic;        /* Magic number identifying file */
	__u32 dqh_version;      /* File version */
};

static int v1_check_quota_file(struct super_block *sb, int type)
{
	struct file *f = sb_dqopt(sb)->files[type];
	struct inode *inode = f->f_dentry->d_inode;
	ulong blocks;
	size_t off; 
	struct v2_disk_dqheader dqhead;
	mm_segment_t fs;
	ssize_t size;
	loff_t offset = 0;
	static const uint quota_magics[] = V2_INITQMAGICS;

	if (!inode->i_size)
		return 0;
	blocks = inode->i_size >> BLOCK_SIZE_BITS;
	off = inode->i_size & (BLOCK_SIZE - 1);
	if ((blocks % sizeof(struct v1_disk_dqblk) * BLOCK_SIZE + off) % sizeof(struct v1_disk_dqblk))
		return 0;
	/* Doublecheck whether we didn't get file with new format - with old quotactl() this could happen */
	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->read(f, (char *)&dqhead, sizeof(struct v2_disk_dqheader), &offset);
	set_fs(fs);
	if (size != sizeof(struct v2_disk_dqheader))
		return 1;	/* Probably not new format */
	if (le32_to_cpu(dqhead.dqh_magic) != quota_magics[type])
		return 1;	/* Definitely not new format */
	printk(KERN_INFO "VFS: %s: Refusing to turn on old quota format on given file. It probably contains newer quota format.\n", kdevname(sb->s_dev));
        return 0;		/* Seems like a new format file -> refuse it */
}

static int v1_read_file_info(struct super_block *sb, int type)
{
	struct quota_info *dqopt = sb_dqopt(sb);
	mm_segment_t fs;
	loff_t offset;
	struct file *filp = dqopt->files[type];
	struct v1_disk_dqblk dqblk;
	int ret;

	down(&dqopt->dqio_sem);
	offset = v1_dqoff(0);
	fs = get_fs();
	set_fs(KERNEL_DS);
	if ((ret = filp->f_op->read(filp, (char *)&dqblk, sizeof(struct v1_disk_dqblk), &offset)) != sizeof(struct v1_disk_dqblk)) {
		if (ret >= 0)
			ret = -EIO;
		goto out;
	}
	ret = 0;
	dqopt->info[type].dqi_igrace = dqblk.dqb_itime ? dqblk.dqb_itime : MAX_IQ_TIME;
	dqopt->info[type].dqi_bgrace = dqblk.dqb_btime ? dqblk.dqb_btime : MAX_DQ_TIME;
out:
	up(&dqopt->dqio_sem);
	set_fs(fs);
	return ret;
}

static int v1_write_file_info(struct super_block *sb, int type)
{
	struct quota_info *dqopt = sb_dqopt(sb);
	mm_segment_t fs;
	struct file *filp = dqopt->files[type];
	struct v1_disk_dqblk dqblk;
	loff_t offset;
	int ret;

	down(&dqopt->dqio_sem);
	dqopt->info[type].dqi_flags &= ~DQF_INFO_DIRTY;
	offset = v1_dqoff(0);
	fs = get_fs();
	set_fs(KERNEL_DS);
	if ((ret = filp->f_op->read(filp, (char *)&dqblk, sizeof(struct v1_disk_dqblk), &offset)) != sizeof(struct v1_disk_dqblk)) {
		if (ret >= 0)
			ret = -EIO;
		goto out;
	}
	dqblk.dqb_itime = dqopt->info[type].dqi_igrace;
	dqblk.dqb_btime = dqopt->info[type].dqi_bgrace;
	offset = v1_dqoff(0);
	ret = filp->f_op->write(filp, (char *)&dqblk, sizeof(struct v1_disk_dqblk), &offset);
	if (ret == sizeof(struct v1_disk_dqblk))
		ret = 0;
	else if (ret > 0)
		ret = -EIO;
out:
	up(&dqopt->dqio_sem);
	set_fs(fs);
	return ret;
}

static struct quota_format_ops v1_format_ops = {
	check_quota_file:	v1_check_quota_file,
	read_file_info:		v1_read_file_info,
	write_file_info:	v1_write_file_info,
	free_file_info:		NULL,
	read_dqblk:		v1_read_dqblk,
	commit_dqblk:		v1_commit_dqblk,
};

static struct quota_format_type v1_quota_format = {
	qf_fmt_id:	QFMT_VFS_OLD,
	qf_ops:		&v1_format_ops,
	qf_owner:	THIS_MODULE
};

static int __init init_v1_quota_format(void)
{
        return register_quota_format(&v1_quota_format);
}

static void __exit exit_v1_quota_format(void)
{
        unregister_quota_format(&v1_quota_format);
}

EXPORT_NO_SYMBOLS;

module_init(init_v1_quota_format);
module_exit(exit_v1_quota_format);

