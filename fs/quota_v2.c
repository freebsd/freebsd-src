/*
 *	vfsv0 quota IO operations on file
 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/dqblk_v2.h>
#include <linux/quotaio_v2.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <asm/byteorder.h>
#include <asm/uaccess.h>

#define __QUOTA_V2_PARANOIA

typedef char *dqbuf_t;

#define GETIDINDEX(id, depth) (((id) >> ((V2_DQTREEDEPTH-(depth)-1)*8)) & 0xff)
#define GETENTRIES(buf) ((struct v2_disk_dqblk *)(((char *)buf)+sizeof(struct v2_disk_dqdbheader)))

/* Check whether given file is really vfsv0 quotafile */
static int v2_check_quota_file(struct super_block *sb, int type)
{
	struct v2_disk_dqheader dqhead;
	struct file *f = sb_dqopt(sb)->files[type];
	mm_segment_t fs;
	ssize_t size;
	loff_t offset = 0;
	static const uint quota_magics[] = V2_INITQMAGICS;
	static const uint quota_versions[] = V2_INITQVERSIONS;
 
	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->read(f, (char *)&dqhead, sizeof(struct v2_disk_dqheader), &offset);
	set_fs(fs);
	if (size != sizeof(struct v2_disk_dqheader))
		return 0;
	if (le32_to_cpu(dqhead.dqh_magic) != quota_magics[type] ||
	    le32_to_cpu(dqhead.dqh_version) != quota_versions[type])
		return 0;
	return 1;
}

/* Read information header from quota file */
static int v2_read_file_info(struct super_block *sb, int type)
{
	mm_segment_t fs;
	struct v2_disk_dqinfo dinfo;
	struct mem_dqinfo *info = sb_dqopt(sb)->info+type;
	struct file *f = sb_dqopt(sb)->files[type];
	ssize_t size;
	loff_t offset = V2_DQINFOOFF;

	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->read(f, (char *)&dinfo, sizeof(struct v2_disk_dqinfo), &offset);
	set_fs(fs);
	if (size != sizeof(struct v2_disk_dqinfo)) {
		printk(KERN_WARNING "Can't read info structure on device %s.\n",
			kdevname(f->f_dentry->d_sb->s_dev));
		return -1;
	}
	info->dqi_bgrace = le32_to_cpu(dinfo.dqi_bgrace);
	info->dqi_igrace = le32_to_cpu(dinfo.dqi_igrace);
	info->dqi_flags = le32_to_cpu(dinfo.dqi_flags);
	info->u.v2_i.dqi_blocks = le32_to_cpu(dinfo.dqi_blocks);
	info->u.v2_i.dqi_free_blk = le32_to_cpu(dinfo.dqi_free_blk);
	info->u.v2_i.dqi_free_entry = le32_to_cpu(dinfo.dqi_free_entry);
	return 0;
}

/* Write information header to quota file */
static int v2_write_file_info(struct super_block *sb, int type)
{
	mm_segment_t fs;
	struct v2_disk_dqinfo dinfo;
	struct mem_dqinfo *info = sb_dqopt(sb)->info+type;
	struct file *f = sb_dqopt(sb)->files[type];
	ssize_t size;
	loff_t offset = V2_DQINFOOFF;

	info->dqi_flags &= ~DQF_INFO_DIRTY;
	dinfo.dqi_bgrace = cpu_to_le32(info->dqi_bgrace);
	dinfo.dqi_igrace = cpu_to_le32(info->dqi_igrace);
	dinfo.dqi_flags = cpu_to_le32(info->dqi_flags & DQF_MASK);
	dinfo.dqi_blocks = cpu_to_le32(info->u.v2_i.dqi_blocks);
	dinfo.dqi_free_blk = cpu_to_le32(info->u.v2_i.dqi_free_blk);
	dinfo.dqi_free_entry = cpu_to_le32(info->u.v2_i.dqi_free_entry);
	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->write(f, (char *)&dinfo, sizeof(struct v2_disk_dqinfo), &offset);
	set_fs(fs);
	if (size != sizeof(struct v2_disk_dqinfo)) {
		printk(KERN_WARNING "Can't write info structure on device %s.\n",
			kdevname(f->f_dentry->d_sb->s_dev));
		return -1;
	}
	return 0;
}

static void disk2memdqb(struct mem_dqblk *m, struct v2_disk_dqblk *d)
{
	m->dqb_ihardlimit = le32_to_cpu(d->dqb_ihardlimit);
	m->dqb_isoftlimit = le32_to_cpu(d->dqb_isoftlimit);
	m->dqb_curinodes = le32_to_cpu(d->dqb_curinodes);
	m->dqb_itime = le64_to_cpu(d->dqb_itime);
	m->dqb_bhardlimit = le32_to_cpu(d->dqb_bhardlimit);
	m->dqb_bsoftlimit = le32_to_cpu(d->dqb_bsoftlimit);
	m->dqb_curspace = le64_to_cpu(d->dqb_curspace);
	m->dqb_btime = le64_to_cpu(d->dqb_btime);
}

static void mem2diskdqb(struct v2_disk_dqblk *d, struct mem_dqblk *m, qid_t id)
{
	d->dqb_ihardlimit = cpu_to_le32(m->dqb_ihardlimit);
	d->dqb_isoftlimit = cpu_to_le32(m->dqb_isoftlimit);
	d->dqb_curinodes = cpu_to_le32(m->dqb_curinodes);
	d->dqb_itime = cpu_to_le64(m->dqb_itime);
	d->dqb_bhardlimit = cpu_to_le32(m->dqb_bhardlimit);
	d->dqb_bsoftlimit = cpu_to_le32(m->dqb_bsoftlimit);
	d->dqb_curspace = cpu_to_le64(m->dqb_curspace);
	d->dqb_btime = cpu_to_le64(m->dqb_btime);
	d->dqb_id = cpu_to_le32(id);
}

static dqbuf_t getdqbuf(void)
{
	dqbuf_t buf = kmalloc(V2_DQBLKSIZE, GFP_KERNEL);
	if (!buf)
		printk(KERN_WARNING "VFS: Not enough memory for quota buffers.\n");
	return buf;
}

static inline void freedqbuf(dqbuf_t buf)
{
	kfree(buf);
}

static ssize_t read_blk(struct file *filp, uint blk, dqbuf_t buf)
{
	mm_segment_t fs;
	ssize_t ret;
	loff_t offset = blk<<V2_DQBLKSIZE_BITS;

	memset(buf, 0, V2_DQBLKSIZE);
	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = filp->f_op->read(filp, (char *)buf, V2_DQBLKSIZE, &offset);
	set_fs(fs);
	return ret;
}

static ssize_t write_blk(struct file *filp, uint blk, dqbuf_t buf)
{
	mm_segment_t fs;
	ssize_t ret;
	loff_t offset = blk<<V2_DQBLKSIZE_BITS;

	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = filp->f_op->write(filp, (char *)buf, V2_DQBLKSIZE, &offset);
	set_fs(fs);
	return ret;

}

/* Remove empty block from list and return it */
static int get_free_dqblk(struct file *filp, struct mem_dqinfo *info)
{
	dqbuf_t buf = getdqbuf();
	struct v2_disk_dqdbheader *dh = (struct v2_disk_dqdbheader *)buf;
	int ret, blk;

	if (!buf)
		return -ENOMEM;
	if (info->u.v2_i.dqi_free_blk) {
		blk = info->u.v2_i.dqi_free_blk;
		if ((ret = read_blk(filp, blk, buf)) < 0)
			goto out_buf;
		info->u.v2_i.dqi_free_blk = le32_to_cpu(dh->dqdh_next_free);
	}
	else {
		memset(buf, 0, V2_DQBLKSIZE);
		if ((ret = write_blk(filp, info->u.v2_i.dqi_blocks, buf)) < 0)	/* Assure block allocation... */
			goto out_buf;
		blk = info->u.v2_i.dqi_blocks++;
	}
	mark_info_dirty(info);
	ret = blk;
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Insert empty block to the list */
static int put_free_dqblk(struct file *filp, struct mem_dqinfo *info, dqbuf_t buf, uint blk)
{
	struct v2_disk_dqdbheader *dh = (struct v2_disk_dqdbheader *)buf;
	int err;

	dh->dqdh_next_free = cpu_to_le32(info->u.v2_i.dqi_free_blk);
	dh->dqdh_prev_free = cpu_to_le32(0);
	dh->dqdh_entries = cpu_to_le16(0);
	info->u.v2_i.dqi_free_blk = blk;
	mark_info_dirty(info);
	if ((err = write_blk(filp, blk, buf)) < 0)	/* Some strange block. We had better leave it... */
		return err;
	return 0;
}

/* Remove given block from the list of blocks with free entries */
static int remove_free_dqentry(struct file *filp, struct mem_dqinfo *info, dqbuf_t buf, uint blk)
{
	dqbuf_t tmpbuf = getdqbuf();
	struct v2_disk_dqdbheader *dh = (struct v2_disk_dqdbheader *)buf;
	uint nextblk = le32_to_cpu(dh->dqdh_next_free), prevblk = le32_to_cpu(dh->dqdh_prev_free);
	int err;

	if (!tmpbuf)
		return -ENOMEM;
	if (nextblk) {
		if ((err = read_blk(filp, nextblk, tmpbuf)) < 0)
			goto out_buf;
		((struct v2_disk_dqdbheader *)tmpbuf)->dqdh_prev_free = dh->dqdh_prev_free;
		if ((err = write_blk(filp, nextblk, tmpbuf)) < 0)
			goto out_buf;
	}
	if (prevblk) {
		if ((err = read_blk(filp, prevblk, tmpbuf)) < 0)
			goto out_buf;
		((struct v2_disk_dqdbheader *)tmpbuf)->dqdh_next_free = dh->dqdh_next_free;
		if ((err = write_blk(filp, prevblk, tmpbuf)) < 0)
			goto out_buf;
	}
	else {
		info->u.v2_i.dqi_free_entry = nextblk;
		mark_info_dirty(info);
	}
	freedqbuf(tmpbuf);
	dh->dqdh_next_free = dh->dqdh_prev_free = cpu_to_le32(0);
	if (write_blk(filp, blk, buf) < 0)	/* No matter whether write succeeds block is out of list */
		printk(KERN_ERR "VFS: Can't write block (%u) with free entries.\n", blk);
	return 0;
out_buf:
	freedqbuf(tmpbuf);
	return err;
}

/* Insert given block to the beginning of list with free entries */
static int insert_free_dqentry(struct file *filp, struct mem_dqinfo *info, dqbuf_t buf, uint blk)
{
	dqbuf_t tmpbuf = getdqbuf();
	struct v2_disk_dqdbheader *dh = (struct v2_disk_dqdbheader *)buf;
	int err;

	if (!tmpbuf)
		return -ENOMEM;
	dh->dqdh_next_free = cpu_to_le32(info->u.v2_i.dqi_free_entry);
	dh->dqdh_prev_free = cpu_to_le32(0);
	if ((err = write_blk(filp, blk, buf)) < 0)
		goto out_buf;
	if (info->u.v2_i.dqi_free_entry) {
		if ((err = read_blk(filp, info->u.v2_i.dqi_free_entry, tmpbuf)) < 0)
			goto out_buf;
		((struct v2_disk_dqdbheader *)tmpbuf)->dqdh_prev_free = cpu_to_le32(blk);
		if ((err = write_blk(filp, info->u.v2_i.dqi_free_entry, tmpbuf)) < 0)
			goto out_buf;
	}
	freedqbuf(tmpbuf);
	info->u.v2_i.dqi_free_entry = blk;
	mark_info_dirty(info);
	return 0;
out_buf:
	freedqbuf(tmpbuf);
	return err;
}

/* Find space for dquot */
static uint find_free_dqentry(struct dquot *dquot, int *err)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info+dquot->dq_type;
	uint blk, i;
	struct v2_disk_dqdbheader *dh;
	struct v2_disk_dqblk *ddquot;
	struct v2_disk_dqblk fakedquot;
	dqbuf_t buf;

	*err = 0;
	if (!(buf = getdqbuf())) {
		*err = -ENOMEM;
		return 0;
	}
	dh = (struct v2_disk_dqdbheader *)buf;
	ddquot = GETENTRIES(buf);
	if (info->u.v2_i.dqi_free_entry) {
		blk = info->u.v2_i.dqi_free_entry;
		if ((*err = read_blk(filp, blk, buf)) < 0)
			goto out_buf;
	}
	else {
		blk = get_free_dqblk(filp, info);
		if ((int)blk < 0) {
			*err = blk;
			return 0;
		}
		memset(buf, 0, V2_DQBLKSIZE);
		info->u.v2_i.dqi_free_entry = blk;	/* This is enough as block is already zeroed and entry list is empty... */
		mark_info_dirty(info);
	}
	if (le16_to_cpu(dh->dqdh_entries)+1 >= V2_DQSTRINBLK)	/* Block will be full? */
		if ((*err = remove_free_dqentry(filp, info, buf, blk)) < 0) {
			printk(KERN_ERR "VFS: find_free_dqentry(): Can't remove block (%u) from entry free list.\n", blk);
			goto out_buf;
		}
	dh->dqdh_entries = cpu_to_le16(le16_to_cpu(dh->dqdh_entries)+1);
	memset(&fakedquot, 0, sizeof(struct v2_disk_dqblk));
	/* Find free structure in block */
	for (i = 0; i < V2_DQSTRINBLK && memcmp(&fakedquot, ddquot+i, sizeof(struct v2_disk_dqblk)); i++);
#ifdef __QUOTA_V2_PARANOIA
	if (i == V2_DQSTRINBLK) {
		printk(KERN_ERR "VFS: find_free_dqentry(): Data block full but it shouldn't.\n");
		*err = -EIO;
		goto out_buf;
	}
#endif
	if ((*err = write_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: find_free_dqentry(): Can't write quota data block %u.\n", blk);
		goto out_buf;
	}
	dquot->dq_off = (blk<<V2_DQBLKSIZE_BITS)+sizeof(struct v2_disk_dqdbheader)+i*sizeof(struct v2_disk_dqblk);
	freedqbuf(buf);
	return blk;
out_buf:
	freedqbuf(buf);
	return 0;
}

/* Insert reference to structure into the trie */
static int do_insert_tree(struct dquot *dquot, uint *treeblk, int depth)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info + dquot->dq_type;
	dqbuf_t buf;
	int ret = 0, newson = 0, newact = 0;
	u32 *ref;
	uint newblk;

	if (!(buf = getdqbuf()))
		return -ENOMEM;
	if (!*treeblk) {
		ret = get_free_dqblk(filp, info);
		if (ret < 0)
			goto out_buf;
		*treeblk = ret;
		memset(buf, 0, V2_DQBLKSIZE);
		newact = 1;
	}
	else {
		if ((ret = read_blk(filp, *treeblk, buf)) < 0) {
			printk(KERN_ERR "VFS: Can't read tree quota block %u.\n", *treeblk);
			goto out_buf;
		}
	}
	ref = (u32 *)buf;
	newblk = le32_to_cpu(ref[GETIDINDEX(dquot->dq_id, depth)]);
	if (!newblk)
		newson = 1;
	if (depth == V2_DQTREEDEPTH-1) {
#ifdef __QUOTA_V2_PARANOIA
		if (newblk) {
			printk(KERN_ERR "VFS: Inserting already present quota entry (block %u).\n", ref[GETIDINDEX(dquot->dq_id, depth)]);
			ret = -EIO;
			goto out_buf;
		}
#endif
		newblk = find_free_dqentry(dquot, &ret);
	}
	else
		ret = do_insert_tree(dquot, &newblk, depth+1);
	if (newson && ret >= 0) {
		ref[GETIDINDEX(dquot->dq_id, depth)] = cpu_to_le32(newblk);
		ret = write_blk(filp, *treeblk, buf);
	}
	else if (newact && ret < 0)
		put_free_dqblk(filp, info, buf, *treeblk);
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Wrapper for inserting quota structure into tree */
static inline int dq_insert_tree(struct dquot *dquot)
{
	int tmp = V2_DQTREEOFF;
	return do_insert_tree(dquot, &tmp, 0);
}

/*
 *	We don't have to be afraid of deadlocks as we never have quotas on quota files...
 */
static int v2_write_dquot(struct dquot *dquot)
{
	int type = dquot->dq_type;
	struct file *filp;
	mm_segment_t fs;
	loff_t offset;
	ssize_t ret;
	struct v2_disk_dqblk ddquot;

	if (!dquot->dq_off)
		if ((ret = dq_insert_tree(dquot)) < 0) {
			printk(KERN_ERR "VFS: Error %d occured while creating quota.\n", ret);
			return ret;
		}
	filp = sb_dqopt(dquot->dq_sb)->files[type];
	offset = dquot->dq_off;
	mem2diskdqb(&ddquot, &dquot->dq_dqb, dquot->dq_id);
	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = filp->f_op->write(filp, (char *)&ddquot, sizeof(struct v2_disk_dqblk), &offset);
	set_fs(fs);
	if (ret != sizeof(struct v2_disk_dqblk)) {
		printk(KERN_WARNING "VFS: dquota write failed on dev %s\n", kdevname(dquot->dq_dev));
		if (ret >= 0)
			ret = -ENOSPC;
	}
	else
		ret = 0;
	dqstats.writes++;
	return ret;
}

/* Free dquot entry in data block */
static int free_dqentry(struct dquot *dquot, uint blk)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info + dquot->dq_type;
	struct v2_disk_dqdbheader *dh;
	dqbuf_t buf = getdqbuf();
	int ret = 0;

	if (!buf)
		return -ENOMEM;
	if (dquot->dq_off >> V2_DQBLKSIZE_BITS != blk) {
		printk(KERN_ERR "VFS: Quota structure has offset to other block (%u) than it should (%u).\n", blk, (uint)(dquot->dq_off >> V2_DQBLKSIZE_BITS));
		goto out_buf;
	}
	if ((ret = read_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: Can't read quota data block %u\n", blk);
		goto out_buf;
	}
	dh = (struct v2_disk_dqdbheader *)buf;
	dh->dqdh_entries = cpu_to_le16(le16_to_cpu(dh->dqdh_entries)-1);
	if (!le16_to_cpu(dh->dqdh_entries)) {	/* Block got free? */
		if ((ret = remove_free_dqentry(filp, info, buf, blk)) < 0 ||
		    (ret = put_free_dqblk(filp, info, buf, blk)) < 0) {
			printk(KERN_ERR "VFS: Can't move quota data block (%u) to free list.\n", blk);
			goto out_buf;
		}
	}
	else {
		memset(buf+(dquot->dq_off & ((1 << V2_DQBLKSIZE_BITS)-1)), 0, sizeof(struct v2_disk_dqblk));
		if (le16_to_cpu(dh->dqdh_entries) == V2_DQSTRINBLK-1) {
			/* Insert will write block itself */
			if ((ret = insert_free_dqentry(filp, info, buf, blk)) < 0) {
				printk(KERN_ERR "VFS: Can't insert quota data block (%u) to free entry list.\n", blk);
				goto out_buf;
			}
		}
		else
			if ((ret = write_blk(filp, blk, buf)) < 0) {
				printk(KERN_ERR "VFS: Can't write quota data block %u\n", blk);
				goto out_buf;
			}
	}
	dquot->dq_off = 0;	/* Quota is now unattached */
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Remove reference to dquot from tree */
static int remove_tree(struct dquot *dquot, uint *blk, int depth)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info + dquot->dq_type;
	dqbuf_t buf = getdqbuf();
	int ret = 0;
	uint newblk;
	u32 *ref = (u32 *)buf;
	
	if (!buf)
		return -ENOMEM;
	if ((ret = read_blk(filp, *blk, buf)) < 0) {
		printk(KERN_ERR "VFS: Can't read quota data block %u\n", *blk);
		goto out_buf;
	}
	newblk = le32_to_cpu(ref[GETIDINDEX(dquot->dq_id, depth)]);
	if (depth == V2_DQTREEDEPTH-1) {
		ret = free_dqentry(dquot, newblk);
		newblk = 0;
	}
	else
		ret = remove_tree(dquot, &newblk, depth+1);
	if (ret >= 0 && !newblk) {
		int i;
		ref[GETIDINDEX(dquot->dq_id, depth)] = cpu_to_le32(0);
		for (i = 0; i < V2_DQBLKSIZE && !buf[i]; i++);	/* Block got empty? */
		if (i == V2_DQBLKSIZE) {
			put_free_dqblk(filp, info, buf, *blk);
			*blk = 0;
		}
		else
			if ((ret = write_blk(filp, *blk, buf)) < 0)
				printk(KERN_ERR "VFS: Can't write quota tree block %u.\n", *blk);
	}
out_buf:
	freedqbuf(buf);
	return ret;	
}

/* Delete dquot from tree */
static int v2_delete_dquot(struct dquot *dquot)
{
	uint tmp = V2_DQTREEOFF;

	if (!dquot->dq_off)	/* Even not allocated? */
		return 0;
	return remove_tree(dquot, &tmp, 0);
}

/* Find entry in block */
static loff_t find_block_dqentry(struct dquot *dquot, uint blk)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	dqbuf_t buf = getdqbuf();
	loff_t ret = 0;
	int i;
	struct v2_disk_dqblk *ddquot = GETENTRIES(buf);

	if (!buf)
		return -ENOMEM;
	if ((ret = read_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: Can't read quota tree block %u.\n", blk);
		goto out_buf;
	}
	if (dquot->dq_id)
		for (i = 0; i < V2_DQSTRINBLK && le32_to_cpu(ddquot[i].dqb_id) != dquot->dq_id; i++);
	else {	/* ID 0 as a bit more complicated searching... */
		struct v2_disk_dqblk fakedquot;

		memset(&fakedquot, 0, sizeof(struct v2_disk_dqblk));
		for (i = 0; i < V2_DQSTRINBLK; i++)
			if (!le32_to_cpu(ddquot[i].dqb_id) && memcmp(&fakedquot, ddquot+i, sizeof(struct v2_disk_dqblk)))
				break;
	}
	if (i == V2_DQSTRINBLK) {
		printk(KERN_ERR "VFS: Quota for id %u referenced but not present.\n", dquot->dq_id);
		ret = -EIO;
		goto out_buf;
	}
	else
		ret = (blk << V2_DQBLKSIZE_BITS) + sizeof(struct v2_disk_dqdbheader) + i * sizeof(struct v2_disk_dqblk);
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Find entry for given id in the tree */
static loff_t find_tree_dqentry(struct dquot *dquot, uint blk, int depth)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	dqbuf_t buf = getdqbuf();
	loff_t ret = 0;
	u32 *ref = (u32 *)buf;

	if (!buf)
		return -ENOMEM;
	if ((ret = read_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: Can't read quota tree block %u.\n", blk);
		goto out_buf;
	}
	ret = 0;
	blk = le32_to_cpu(ref[GETIDINDEX(dquot->dq_id, depth)]);
	if (!blk)	/* No reference? */
		goto out_buf;
	if (depth < V2_DQTREEDEPTH-1)
		ret = find_tree_dqentry(dquot, blk, depth+1);
	else
		ret = find_block_dqentry(dquot, blk);
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Find entry for given id in the tree - wrapper function */
static inline loff_t find_dqentry(struct dquot *dquot)
{
	return find_tree_dqentry(dquot, V2_DQTREEOFF, 0);
}

static int v2_read_dquot(struct dquot *dquot)
{
	int type = dquot->dq_type;
	struct file *filp;
	mm_segment_t fs;
	loff_t offset;
	struct v2_disk_dqblk ddquot;
	int ret = 0;

	filp = sb_dqopt(dquot->dq_sb)->files[type];

#ifdef __QUOTA_V2_PARANOIA
	if (!filp || !dquot->dq_sb) {	/* Invalidated quota? */
		printk(KERN_ERR "VFS: Quota invalidated while reading!\n");
		return -EIO;
	}
#endif
	offset = find_dqentry(dquot);
	if (offset <= 0) {	/* Entry not present? */
		if (offset < 0)
			printk(KERN_ERR "VFS: Can't read quota structure for id %u.\n", dquot->dq_id);
		dquot->dq_off = 0;
		dquot->dq_flags |= DQ_FAKE;
		memset(&dquot->dq_dqb, 0, sizeof(struct mem_dqblk));
		ret = offset;
	}
	else {
		dquot->dq_off = offset;
		fs = get_fs();
		set_fs(KERNEL_DS);
		if ((ret = filp->f_op->read(filp, (char *)&ddquot, sizeof(struct v2_disk_dqblk), &offset)) != sizeof(struct v2_disk_dqblk)) {
			if (ret >= 0)
				ret = -EIO;
			printk(KERN_ERR "VFS: Error while reading quota structure for id %u.\n", dquot->dq_id);
			memset(&ddquot, 0, sizeof(struct v2_disk_dqblk));
		}
		else
			ret = 0;
		set_fs(fs);
		disk2memdqb(&dquot->dq_dqb, &ddquot);
	}
	dqstats.reads++;
	return ret;
}

/* Commit changes of dquot to disk - it might also mean deleting it when quota became fake one and user has no blocks... */
static int v2_commit_dquot(struct dquot *dquot)
{
	/* We clear the flag everytime so we don't loop when there was an IO error... */
	dquot->dq_flags &= ~DQ_MOD;
	if (dquot->dq_flags & DQ_FAKE && !(dquot->dq_dqb.dqb_curinodes | dquot->dq_dqb.dqb_curspace))
		return v2_delete_dquot(dquot);
	else
		return v2_write_dquot(dquot);
}

static struct quota_format_ops v2_format_ops = {
	check_quota_file:	v2_check_quota_file,
	read_file_info:		v2_read_file_info,
	write_file_info:	v2_write_file_info,
	free_file_info:		NULL,
	read_dqblk:		v2_read_dquot,
	commit_dqblk:		v2_commit_dquot,
};

static struct quota_format_type v2_quota_format = {
	qf_fmt_id:	QFMT_VFS_V0,
	qf_ops:		&v2_format_ops,
	qf_owner:	THIS_MODULE
};

static int __init init_v2_quota_format(void)
{
	return register_quota_format(&v2_quota_format);
}

static void __exit exit_v2_quota_format(void)
{
	unregister_quota_format(&v2_quota_format);
}

EXPORT_NO_SYMBOLS;

module_init(init_v2_quota_format);
module_exit(exit_v2_quota_format);
