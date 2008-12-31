/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <dumbbell@FreeBSD.org>
 * 
 * $FreeBSD: src/sys/gnu/fs/reiserfs/reiserfs_inode.c,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

static b_strategy_t reiserfs_bufstrategy;

/*
 * Buffer operations for ReiserFS vnodes.
 * We punt on VOP_BMAP, so we need to do strategy on the file's vnode
 * rather than the underlying device's.
 */
static struct buf_ops reiserfs_vnbufops = {
	.bop_name	= "ReiserFS",
	.bop_strategy	= reiserfs_bufstrategy,
};

/* Default io size devuned in super.c */
extern int reiserfs_default_io_size;
void inode_set_bytes(struct reiserfs_node *ip, off_t bytes);

/* Args for the create parameter of reiserfs_get_block */
#define GET_BLOCK_NO_CREATE	0  /* Don't create new blocks or convert
				      tails */
#define GET_BLOCK_CREATE	1  /* Add anything you need to find block */
#define GET_BLOCK_NO_HOLE	2  /* Return ENOENT for file holes */
#define GET_BLOCK_READ_DIRECT	4  /* Read the tail if indirect item not
				      found */
#define GET_BLOCK_NO_ISEM	8  /* i_sem is not held, don't preallocate */
#define GET_BLOCK_NO_DANGLE	16 /* Don't leave any transactions running */

/* -------------------------------------------------------------------
 * vnode operations
 * -------------------------------------------------------------------*/

int
reiserfs_read(struct vop_read_args *ap)
{
	struct uio *uio;
	struct vnode *vp;
	struct reiserfs_node *ip;
	struct reiserfs_sb_info *sbi;

	int error;
	long size;
	daddr_t lbn;
	off_t bytesinfile, offset;

	uio = ap->a_uio;
	vp  = ap->a_vp;
	ip  = VTOI(vp);
	sbi = ip->i_reiserfs;

	size = sbi->s_blocksize;

	for (error = 0; uio->uio_resid > 0;) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;

		/* Compute the logical block number and its offset */
		lbn    = uio->uio_offset / size;
		offset = uio->uio_offset % size;
		reiserfs_log(LOG_DEBUG, "logical block number: %ju\n",
		    (intmax_t)lbn);
		reiserfs_log(LOG_DEBUG, "block offset:         %ju\n",
		    (intmax_t)offset);

		/* Read file blocks */
		reiserfs_log(LOG_DEBUG, "reiserfs_get_block(%ju)\n",
		    (intmax_t)lbn);
		if ((error = reiserfs_get_block(ip, lbn, offset, uio)) != 0) {
			reiserfs_log(LOG_DEBUG,
			    "reiserfs_get_block returned the error %d\n",
			    error);
			break;
		}
	}

	return (error);
}

static void
reiserfs_bufstrategy(struct bufobj *bo, struct buf *bp)
{
	struct vnode *vp;
	int rc;

	vp = bo->bo_private;
	KASSERT(bo == &vp->v_bufobj, ("BO/VP mismatch: vp %p bo %p != %p",
	    vp, &vp->v_bufobj, bo));
	rc = VOP_STRATEGY(vp, bp);
	KASSERT(rc == 0, ("ReiserFS VOP_STRATEGY failed: bp=%p, "
	    "vp=%p, rc=%d", bp, vp, rc));
}

int
reiserfs_inactive(struct vop_inactive_args *ap)
{
	int error;
	struct vnode *vp;
	struct thread *td;
	struct reiserfs_node *ip;

	error = 0;
	vp = ap->a_vp;
	td = ap->a_td;
	ip = VTOI(vp);

	reiserfs_log(LOG_DEBUG, "deactivating inode used %d times\n",
	    vp->v_usecount);
	if (prtactive && vrefcnt(vp) != 0)
		vprint("ReiserFS/reclaim: pushing active", vp);

#if 0
	/* Ignore inodes related to stale file handles. */
	if (ip->i_mode == 0)
		goto out;

out:
#endif

	/*
	 * If we are done with the inode, reclaim it so that it can be reused
	 * immediately.
	 */
	if (ip->i_mode == 0) {
		reiserfs_log(LOG_DEBUG, "recyling\n");
		vrecycle(vp, td);
	}

	return (error);
}

int
reiserfs_reclaim(struct vop_reclaim_args *ap)
{
	struct reiserfs_node *ip;
	struct vnode *vp;

	vp = ap->a_vp;

	reiserfs_log(LOG_DEBUG, "reclaiming inode used %d times\n",
	    vp->v_usecount);
	if (prtactive && vrefcnt(vp) != 0)
		vprint("ReiserFS/reclaim: pushing active", vp);
	ip = VTOI(vp);

	/* XXX Update this node (write to the disk) */

	/* Remove the inode from its hash chain. */
	vfs_hash_remove(vp);

	reiserfs_log(LOG_DEBUG, "free private data\n");
	FREE(vp->v_data, M_REISERFSNODE);
	vp->v_data = NULL;
	vnode_destroy_vobject(vp);

	return (0);
}

/* -------------------------------------------------------------------
 * Functions from linux/fs/reiserfs/inode.c
 * -------------------------------------------------------------------*/

static void
_make_cpu_key(struct cpu_key *key, int version,
    uint32_t dirid, uint32_t objectid, off_t offset, int type, int length)
{

	key->version = version;

	key->on_disk_key.k_dir_id   = dirid;
	key->on_disk_key.k_objectid = objectid;
	set_cpu_key_k_offset(key, offset);
	set_cpu_key_k_type(key, type);
	key->key_length = length;
}

/*
 * Take base of inode_key (it comes from inode always) (dirid, objectid)
 * and version from an inode, set offset and type of key
 */
void
make_cpu_key(struct cpu_key *key, struct reiserfs_node *ip, off_t offset,
    int type, int length)
{

	_make_cpu_key(key, get_inode_item_key_version(ip),
	    le32toh(INODE_PKEY(ip)->k_dir_id),
	    le32toh(INODE_PKEY(ip)->k_objectid),
	    offset, type, length);
}

int
reiserfs_get_block(struct reiserfs_node *ip, long block, off_t offset,
    struct uio *uio)
{
	caddr_t blk = NULL, p;
	struct cpu_key key;
	/* unsigned long offset; */
	INITIALIZE_PATH(path);
	struct buf *bp, *blk_bp;
	struct item_head *ih;
	struct reiserfs_sb_info *sbi;
	int blocknr, chars, done = 0, ret = 0, args = 0;

	sbi = ip->i_reiserfs;

	/* Prepare the key to look for the 'block'-th block of file */
	reiserfs_log(LOG_DEBUG, "prepare cpu key\n");
	make_cpu_key(&key, ip, (off_t)block * sbi->s_blocksize + 1, TYPE_ANY, 3);

	/* research: */
	reiserfs_log(LOG_DEBUG, "search for position\n");
	if (search_for_position_by_key(sbi, &key, &path) != POSITION_FOUND) {
		reiserfs_log(LOG_DEBUG, "position not found\n");
		pathrelse(&path);
#if 0
		if (blk)
			kunmap(bh_result->b_page);
#endif
		/*
		 * We do not return ENOENT if there is a hole but page is
		 * uptodate, because it means that there is some MMAPED data
		 * associated with it that is yet to be written to disk.
		 */
		if ((args & GET_BLOCK_NO_HOLE)/* &&
		    !PageUptodate(bh_result->b_page)*/)
			return (ENOENT);
		return (0);
	}
	reiserfs_log(LOG_DEBUG, "position found\n");

	bp = get_last_bp(&path);
	ih = get_ih(&path);

	if (is_indirect_le_ih(ih)) {
		off_t xfersize;
		uint32_t *ind_item = (uint32_t *)B_I_PITEM(bp, ih);

		reiserfs_log(LOG_DEBUG, "item is INDIRECT\n");

		blocknr = get_block_num(ind_item, path.pos_in_item);
		reiserfs_log(LOG_DEBUG, "block number: %d "
		    "(ind_item=%p, pos_in_item=%u)\n",
		    blocknr, ind_item, path.pos_in_item);

		xfersize = MIN(sbi->s_blocksize - offset,
		    ip->i_size - uio->uio_offset);
		xfersize = MIN(xfersize, uio->uio_resid);

		if (blocknr) {
			ret = bread(sbi->s_devvp,
			    blocknr * btodb(sbi->s_blocksize),
			    sbi->s_blocksize, NOCRED, &blk_bp);
			reiserfs_log(LOG_DEBUG, "xfersize: %ju\n",
			    (intmax_t)xfersize);
			ret = uiomove(blk_bp->b_data + offset, xfersize, uio);
			brelse(blk_bp);
		} else {
			/*
			 * We do not return ENOENT if there is a hole but
			 * page is uptodate, because it means That there
			 * is some MMAPED data associated with it that
			 * is yet to be written to disk.
			 */
			if ((args & GET_BLOCK_NO_HOLE)/* &&
			    !PageUptodate(bh_result->b_page)*/)
				ret = (ENOENT);

			/* Skip this hole */
			uio->uio_resid  -= xfersize;
			uio->uio_offset += xfersize;
		}

		pathrelse(&path);
		return (ret);
	}

	reiserfs_log(LOG_DEBUG, "item should be DIRECT\n");

#if 0
	/* Requested data are in direct item(s) */
	if (!(args & GET_BLOCK_READ_DIRECT)) {
		/*
		 * We are called by bmap. FIXME: we can not map block of
		 * file when it is stored in direct item(s)
		 */
		pathrelse(&path);
#if 0
		if (blk)
			kunmap(bh_result->b_page);
#endif
		return (ENOENT);
	}
#endif

#if 0
	/*
	 * If we've got a direct item, and the buffer or page was uptodate, we
	 * don't want to pull data off disk again. Skip to the end, where we
	 * map the buffer and return
	 */
	if (buffer_uptodate(bh_result)) {
		goto finished;
	} else
		/*
		 * grab_tail_page can trigger calls to reiserfs_get_block
		 * on up to date pages without any buffers. If the page
		 * is up to date, we don't want read old data off disk.
		 * Set the up to date bit on the buffer instead and jump
		 * to the end
		 */
		if (!bh_result->b_page || PageUptodate(bh_result->b_page)) {
			set_buffer_uptodate(bh_result);
			goto finished;
		}
#endif

#if 0
	/* Read file tail into part of page */
	offset = (cpu_key_k_offset(&key) - 1) & (PAGE_CACHE_SIZE - 1);
	fs_gen = get_generation(ip->i_reiserfs);
	copy_item_head(&tmp_ih, ih);
#endif

#if 0
	/*
	 * We only want to kmap if we are reading the tail into the page. this
	 * is not the common case, so we don't kmap until we are sure we need
	 * to. But, this means the item might move if kmap schedules
	 */
	if (!blk) {
		blk = (char *)kmap(bh_result->b_page);
		if (fs_changed (fs_gen, sbi) && item_moved(&tmp_ih, &path))
			goto research;
	}
	blk += offset;
	memset(blk, 0, sbi->s_blocksize);
#endif
	if (!blk) {
		reiserfs_log(LOG_DEBUG, "allocating buffer\n");
		blk = malloc(ip->i_size, M_REISERFSNODE, M_WAITOK | M_ZERO);
		if (!blk)
			return (ENOMEM);
	}
	/* p += offset; */

	p = blk;
	do {
		if (!is_direct_le_ih(ih)) {
			reiserfs_log(LOG_ERR, "BUG\n");
			return (ENOENT); /* XXX Wrong error code */
		}

		/*
		 * Make sure we don't read more bytes than actually exist
		 * in the file. This can happen in odd cases where i_size
		 * isn't correct, and when direct item padding results in
		 * a few extra bytes at the end of the direct item
		 */
		if ((le_ih_k_offset(ih) + path.pos_in_item) > ip->i_size)
			break;

		if ((le_ih_k_offset(ih) - 1 + ih_item_len(ih)) > ip->i_size) {
			chars = ip->i_size - (le_ih_k_offset(ih) - 1) -
			    path.pos_in_item;
			done  = 1;
		} else {
			chars = ih_item_len(ih) - path.pos_in_item;
		}
		reiserfs_log(LOG_DEBUG, "copying %d bytes\n", chars);
		memcpy(p, B_I_PITEM(bp, ih) + path.pos_in_item, chars);
		if (done) {
			reiserfs_log(LOG_DEBUG, "copy done\n");
			break;
		}

		p += chars;

		if (PATH_LAST_POSITION(&path) != (B_NR_ITEMS(bp) - 1))
			/*
			 * We done, if read direct item is not the last
			 * item of node
			 * FIXME: we could try to check right delimiting
			 * key to see whether direct item continues in
			 * the right neighbor or rely on i_size
			 */
			break;

		/* Update key to look for the next piece */
		set_cpu_key_k_offset(&key, cpu_key_k_offset(&key) + chars);
		if (search_for_position_by_key(sbi, &key, &path) !=
		    POSITION_FOUND)
			/*
			 * We read something from tail, even if now we got
			 * IO_ERROR
			 */
			break;

		bp = get_last_bp(&path);
		ih = get_ih(&path);
	} while (1);

	/* finished: */
	pathrelse(&path);
	/*
	 * This buffer has valid data, but isn't valid for io. mapping it to
	 * block #0 tells the rest of reiserfs it just has a tail in it
	 */
	ret = uiomove(blk, ip->i_size, uio);
	free(blk, M_REISERFSNODE);
	return (ret);
}

/*
 * Compute real number of used bytes by file
 * Following three functions can go away when we'll have enough space in
 * stat item
 */
static int
real_space_diff(struct reiserfs_node *ip, int sd_size)
{
	int bytes;
	off_t blocksize = ip->i_reiserfs->s_blocksize;

	if (S_ISLNK(ip->i_mode) || S_ISDIR(ip->i_mode))
		return (sd_size);

	/* End of file is also in full block with indirect reference, so round
	 * up to the next block.
	 *
	 * There is just no way to know if the tail is actually packed on the
	 * file, so we have to assume it isn't. When we pack the tail, we add
	 * 4 bytes to pretend there really is an unformatted node pointer. */
	bytes = ((ip->i_size + (blocksize - 1)) >>
	    ip->i_reiserfs->s_blocksize_bits) * UNFM_P_SIZE + sd_size;

	return (bytes);
}

static inline off_t
to_real_used_space(struct reiserfs_node *ip, unsigned long blocks, int sd_size)
{

	if (S_ISLNK(ip->i_mode) || S_ISDIR(ip->i_mode)) {
		return ip->i_size + (off_t)(real_space_diff(ip, sd_size));
	}

	return ((off_t)real_space_diff(ip, sd_size)) + (((off_t)blocks) << 9);
}

void
inode_set_bytes(struct reiserfs_node *ip, off_t bytes)
{

	ip->i_blocks = bytes >> 9;
	ip->i_bytes  = bytes & 511;
}

/* Called by read_locked_inode */
static void
init_inode(struct reiserfs_node *ip, struct path *path)
{
	struct buf *bp;
	struct item_head *ih;
	uint32_t rdev;

	bp = PATH_PLAST_BUFFER(path);
	ih = PATH_PITEM_HEAD(path);

	reiserfs_log(LOG_DEBUG, "copy the key (objectid=%d, dirid=%d)\n",
	    ih->ih_key.k_objectid, ih->ih_key.k_dir_id);
	copy_key(INODE_PKEY(ip), &(ih->ih_key));
	/* ip->i_blksize = reiserfs_default_io_size; */

	reiserfs_log(LOG_DEBUG, "reset some inode structure members\n");
	REISERFS_I(ip)->i_flags = 0;
#if 0
	REISERFS_I(ip)->i_prealloc_block = 0;
	REISERFS_I(ip)->i_prealloc_count = 0;
	REISERFS_I(ip)->i_trans_id = 0;
	REISERFS_I(ip)->i_jl = NULL;
	REISERFS_I(ip)->i_acl_access = NULL;
	REISERFS_I(ip)->i_acl_default = NULL;
#endif

	if (stat_data_v1(ih)) {
		reiserfs_log(LOG_DEBUG, "reiserfs/init_inode: stat data v1\n");
		struct stat_data_v1 *sd;
		unsigned long blocks;

		sd = (struct stat_data_v1 *)B_I_PITEM(bp, ih);
		
		reiserfs_log(LOG_DEBUG,
		    "reiserfs/init_inode: filling more members\n");
		set_inode_item_key_version(ip, KEY_FORMAT_3_5);
		set_inode_sd_version(ip, STAT_DATA_V1);
		ip->i_mode          = sd_v1_mode(sd);
		ip->i_nlink         = sd_v1_nlink(sd);
		ip->i_uid           = sd_v1_uid(sd);
		ip->i_gid           = sd_v1_gid(sd);
		ip->i_size          = sd_v1_size(sd);
		ip->i_atime.tv_sec  = sd_v1_atime(sd);
		ip->i_mtime.tv_sec  = sd_v1_mtime(sd);
		ip->i_ctime.tv_sec  = sd_v1_ctime(sd);
		ip->i_atime.tv_nsec = 0;
		ip->i_ctime.tv_nsec = 0;
		ip->i_mtime.tv_nsec = 0;

		reiserfs_log(LOG_DEBUG, "  mode  = %08x\n", ip->i_mode);
		reiserfs_log(LOG_DEBUG, "  nlink = %d\n", ip->i_nlink);
		reiserfs_log(LOG_DEBUG, "  owner = %d:%d\n", ip->i_uid,
		    ip->i_gid);
		reiserfs_log(LOG_DEBUG, "  size  = %ju\n",
		    (intmax_t)ip->i_size);
		reiserfs_log(LOG_DEBUG, "  atime = %jd\n",
		    (intmax_t)ip->i_atime.tv_sec);
		reiserfs_log(LOG_DEBUG, "  mtime = %jd\n",
		    (intmax_t)ip->i_mtime.tv_sec);
		reiserfs_log(LOG_DEBUG, "  ctime = %jd\n",
		    (intmax_t)ip->i_ctime.tv_sec);

		ip->i_blocks     = sd_v1_blocks(sd);
		ip->i_generation = le32toh(INODE_PKEY(ip)->k_dir_id);
		blocks = (ip->i_size + 511) >> 9;
		blocks = _ROUND_UP(blocks, ip->i_reiserfs->s_blocksize >> 9);
		if (ip->i_blocks > blocks) {
			/*
			 * There was a bug in <= 3.5.23 when i_blocks could
			 * take negative values. Starting from 3.5.17 this
			 * value could even be stored in stat data. For such
			 * files we set i_blocks based on file size. Just 2
			 * notes: this can be wrong for sparce files. On-disk
			 * value will be only updated if file's inode will
			 * ever change.
			 */
			ip->i_blocks = blocks;
		}

		rdev = sd_v1_rdev(sd);
		REISERFS_I(ip)->i_first_direct_byte =
		    sd_v1_first_direct_byte(sd);

		/*
		 * An early bug in the quota code can give us an odd number
		 * for the block count. This is incorrect, fix it here.
		 */
		if (ip->i_blocks & 1) {
			ip->i_blocks++ ;
		}
		inode_set_bytes(ip, to_real_used_space(ip, ip->i_blocks,
		    SD_V1_SIZE));

		/*
		 * nopack is initially zero for v1 objects. For v2 objects,
		 * nopack is initialised from sd_attrs
		 */
		REISERFS_I(ip)->i_flags &= ~i_nopack_mask;
		reiserfs_log(LOG_DEBUG, "...done\n");
	} else {
		reiserfs_log(LOG_DEBUG, "stat data v2\n");
		/*
		 * New stat data found, but object may have old items
		 * (directories and symlinks)
		 */
		struct stat_data *sd = (struct stat_data *)B_I_PITEM(bp, ih);

		reiserfs_log(LOG_DEBUG, "filling more members\n");
		ip->i_mode          = sd_v2_mode(sd);
		ip->i_nlink         = sd_v2_nlink(sd);
		ip->i_uid           = sd_v2_uid(sd);
		ip->i_size          = sd_v2_size(sd);
		ip->i_gid           = sd_v2_gid(sd);
		ip->i_mtime.tv_sec  = sd_v2_mtime(sd);
		ip->i_atime.tv_sec  = sd_v2_atime(sd);
		ip->i_ctime.tv_sec  = sd_v2_ctime(sd);
		ip->i_ctime.tv_nsec = 0;
		ip->i_mtime.tv_nsec = 0;
		ip->i_atime.tv_nsec = 0;

		reiserfs_log(LOG_DEBUG, "  mode  = %08x\n", ip->i_mode);
		reiserfs_log(LOG_DEBUG, "  nlink = %d\n", ip->i_nlink);
		reiserfs_log(LOG_DEBUG, "  owner = %d:%d\n", ip->i_uid,
		    ip->i_gid);
		reiserfs_log(LOG_DEBUG, "  size  = %ju\n",
		    (intmax_t)ip->i_size);
		reiserfs_log(LOG_DEBUG, "  atime = %jd\n",
		    (intmax_t)ip->i_atime.tv_sec);
		reiserfs_log(LOG_DEBUG, "  mtime = %jd\n",
		    (intmax_t)ip->i_mtime.tv_sec);
		reiserfs_log(LOG_DEBUG, "  ctime = %jd\n",
		    (intmax_t)ip->i_ctime.tv_sec);

		ip->i_blocks = sd_v2_blocks(sd);
		rdev         = sd_v2_rdev(sd);
		reiserfs_log(LOG_DEBUG, "  blocks = %u\n", ip->i_blocks);

		if (S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode))
			ip->i_generation = le32toh(INODE_PKEY(ip)->k_dir_id);
		else
			ip->i_generation = sd_v2_generation(sd);

		if (S_ISDIR(ip->i_mode) || S_ISLNK(ip->i_mode))
			set_inode_item_key_version(ip, KEY_FORMAT_3_5);
		else
			set_inode_item_key_version(ip, KEY_FORMAT_3_6);

		REISERFS_I(ip)->i_first_direct_byte = 0;
		set_inode_sd_version(ip, STAT_DATA_V2);
		inode_set_bytes(ip, to_real_used_space(ip, ip->i_blocks,
		    SD_V2_SIZE));

		/*
		 * Read persistent inode attributes from sd and initalise
		 * generic inode flags from them
		 */
		REISERFS_I(ip)->i_attrs = sd_v2_attrs(sd);
		sd_attrs_to_i_attrs(sd_v2_attrs(sd), ip);
		reiserfs_log(LOG_DEBUG, "...done\n");
	}

	pathrelse(path);
	if (S_ISREG(ip->i_mode)) {
		reiserfs_log(LOG_DEBUG, "this inode is a regular file\n");
		//ip->i_op = &reiserfs_file_ip_operations;
		//ip->i_fop = &reiserfs_file_operations;
		//ip->i_mapping->a_ops = &reiserfs_address_space_operations ;
	} else if (S_ISDIR(ip->i_mode)) {
		reiserfs_log(LOG_DEBUG, "this inode is a directory\n");
		//ip->i_op = &reiserfs_dir_ip_operations;
		//ip->i_fop = &reiserfs_dir_operations;
	} else if (S_ISLNK(ip->i_mode)) {
		reiserfs_log(LOG_DEBUG, "this inode is a symlink\n");
		//ip->i_op = &reiserfs_symlink_ip_operations;
		//ip->i_mapping->a_ops = &reiserfs_address_space_operations;
	} else {
		reiserfs_log(LOG_DEBUG, "this inode is something unknown in "
		    "this universe\n");
		ip->i_blocks = 0;
		//ip->i_op = &reiserfs_special_ip_operations;
		//init_special_ip(ip, ip->i_mode, new_decode_dev(rdev));
	}
}

/*
 * reiserfs_read_locked_inode is called to read the inode off disk, and
 * it does a make_bad_inode when things go wrong. But, we need to make
 * sure and clear the key in the private portion of the inode, otherwise
 * a corresponding iput might try to delete whatever object the inode
 * last represented.
 */
static void
reiserfs_make_bad_inode(struct reiserfs_node *ip) {

	memset(INODE_PKEY(ip), 0, KEY_SIZE);
	//make_bad_inode(inode);
}

void
reiserfs_read_locked_inode(struct reiserfs_node *ip,
    struct reiserfs_iget_args *args)
{
	INITIALIZE_PATH(path_to_sd);
	struct cpu_key key;
	unsigned long dirino;
	int retval;

	dirino = args->dirid;

	/*
	 * Set version 1, version 2 could be used too, because stat data
	 * key is the same in both versions
	 */
	key.version = KEY_FORMAT_3_5;
	key.on_disk_key.k_dir_id = dirino;
	key.on_disk_key.k_objectid = ip->i_number;
	key.on_disk_key.u.k_offset_v1.k_offset = SD_OFFSET;
	key.on_disk_key.u.k_offset_v1.k_uniqueness = SD_UNIQUENESS;

	/* Look for the object's stat data */
	retval = search_item(ip->i_reiserfs, &key, &path_to_sd);
	if (retval == IO_ERROR) {
		reiserfs_log(LOG_ERR,
		    "I/O failure occured trying to find stat"
		    "data %u/%u\n",
		    key.on_disk_key.k_dir_id, key.on_disk_key.k_objectid);
		reiserfs_make_bad_inode(ip);
		return;
	}
	if (retval != ITEM_FOUND) {
		/*
		 * A stale NFS handle can trigger this without it being
		 * an error
		 */
		reiserfs_log(LOG_ERR,
		    "item not found (objectid=%u, dirid=%u)\n",
		    key.on_disk_key.k_objectid, key.on_disk_key.k_dir_id);
		pathrelse(&path_to_sd);
		reiserfs_make_bad_inode(ip);
		ip->i_nlink = 0;
		return;
	}

	init_inode(ip, &path_to_sd);

	/*
	 * It is possible that knfsd is trying to access inode of a file
	 * that is being removed from the disk by some other thread. As
	 * we update sd on unlink all that is required is to check for
	 * nlink here. This bug was first found by Sizif when debugging
	 * SquidNG/Butterfly, forgotten, and found again after Philippe
	 * Gramoulle <philippe.gramoulle@mmania.com> reproduced it.
	 * 
	 * More logical fix would require changes in fs/inode.c:iput() to
	 * remove inode from hash-table _after_ fs cleaned disk stuff up and
	 * in iget() to return NULL if I_FREEING inode is found in hash-table.
	 */
	/*
	 * Currently there is one place where it's ok to meet inode with
	 * nlink == 0: processing of open-unlinked and half-truncated files
	 * during mount (fs/reiserfs/super.c:finish_unfinished()).
	 */
	if((ip->i_nlink == 0) &&
	    !REISERFS_SB(ip->i_reiserfs)->s_is_unlinked_ok ) {
		reiserfs_log(LOG_WARNING, "dead inode read from disk. This is "
		    "likely to be race with knfsd. Ignore");
		reiserfs_make_bad_inode(ip);
	}

	/* Init inode should be relsing */
	reiserfs_check_path(&path_to_sd);
}

int
reiserfs_iget(
    struct mount *mp, const struct cpu_key *key,
    struct vnode **vpp, struct thread *td)
{
	int error, flags;
	struct cdev *dev;
	struct vnode *vp;
	struct reiserfs_node *ip;
	struct reiserfs_mount *rmp;

	struct reiserfs_iget_args args;

	//restart:
	/* Check if the inode cache contains it */
	// XXX LK_EXCLUSIVE ?
	flags = LK_EXCLUSIVE;
	error = vfs_hash_get(mp, key->on_disk_key.k_objectid, flags,
	    td, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	rmp = VFSTOREISERFS(mp);
	dev = rmp->rm_dev;

	/*
	 * If this MALLOC() is performed after the getnewvnode() it might
	 * block, leaving a vnode with a NULL v_data to be found by
	 * reiserfs_sync() if a sync happens to fire right then, which
	 * will cause a panic because reiserfs_sync() blindly dereferences
	 * vp->v_data (as well it should).
	 */
	reiserfs_log(LOG_DEBUG, "malloc(struct reiserfs_node)\n");
	ip = malloc(sizeof(struct reiserfs_node), M_REISERFSNODE,
	    M_WAITOK | M_ZERO);

	/* Allocate a new vnode/inode. */
	reiserfs_log(LOG_DEBUG, "getnewvnode\n");
	if ((error =
	    getnewvnode("reiserfs", mp, &reiserfs_vnodeops, &vp)) != 0) {
		*vpp = NULL;
		free(ip, M_REISERFSNODE);
		reiserfs_log(LOG_DEBUG, "getnewvnode FAILED\n");
		return (error);
	}

	args.dirid = key->on_disk_key.k_dir_id;
	args.objectid = key->on_disk_key.k_objectid;

	reiserfs_log(LOG_DEBUG, "filling *ip\n");
	vp->v_data     = ip;
	ip->i_vnode    = vp;
	ip->i_dev      = dev;
	ip->i_number   = args.objectid;
	ip->i_ino      = args.dirid;
	ip->i_reiserfs = rmp->rm_reiserfs;

	vp->v_bufobj.bo_ops = &reiserfs_vnbufops;
	vp->v_bufobj.bo_private = vp;

	/* If this is the root node, set the VV_ROOT flag */
	if (ip->i_number == REISERFS_ROOT_OBJECTID &&
	    ip->i_ino == REISERFS_ROOT_PARENT_OBJECTID)
		vp->v_vflag |= VV_ROOT;

#if 0
	if (VOP_LOCK(vp, LK_EXCLUSIVE, td) != 0)
		panic("reiserfs/iget: unexpected lock failure");

	/*
	 * Exclusively lock the vnode before adding to hash. Note, that we
	 * must not release nor downgrade the lock (despite flags argument
	 * says) till it is fully initialized.
	 */
	lockmgr(vp->v_vnlock, LK_EXCLUSIVE, (struct mtx *)0, td);
#endif

	lockmgr(vp->v_vnlock, LK_EXCLUSIVE, NULL, td);
	error = insmntque(vp, mp);
	if (error != 0) {
		free(ip, M_REISERFSNODE);
		*vpp = NULL;
		reiserfs_log(LOG_DEBUG, "insmntque FAILED\n");
		return (error);
	}
	error = vfs_hash_insert(vp, key->on_disk_key.k_objectid, flags,
	    td, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	/* Read the inode */
	reiserfs_log(LOG_DEBUG, "call reiserfs_read_locked_inode ("
	    "objectid=%d,dirid=%d)\n", args.objectid, args.dirid);
	reiserfs_read_locked_inode(ip, &args);

	ip->i_devvp = rmp->rm_devvp;

	switch(vp->v_type = IFTOVT(ip->i_mode)) {
	case VBLK:
		reiserfs_log(LOG_DEBUG, "vnode type VBLK\n");
		vp->v_op = &reiserfs_specops;
		break;
#if 0
	case VCHR:
		reiserfs_log(LOG_DEBUG, "vnode type VCHR\n");
		vp->v_op = &reiserfs_specops;
		vp = addaliasu(vp, ip->i_rdev);
		ip->i_vnode = vp;
		break;
	case VFIFO:
		reiserfs_log(LOG_DEBUG, "vnode type VFIFO\n");
		vp->v_op = reiserfs_fifoop_p;
		break;
#endif
	default:
		break;
	}

	*vpp = vp;
	return (0);
}

void
sd_attrs_to_i_attrs(uint16_t sd_attrs, struct reiserfs_node *ip)
{

	if (reiserfs_attrs(ip->i_reiserfs)) {
#if 0
		if (sd_attrs & REISERFS_SYNC_FL)
			ip->i_flags |= S_SYNC;
		else
			ip->i_flags &= ~S_SYNC;
#endif
		if (sd_attrs & REISERFS_IMMUTABLE_FL)
			ip->i_flags |= IMMUTABLE;
		else
			ip->i_flags &= ~IMMUTABLE;
		if (sd_attrs & REISERFS_APPEND_FL)
			ip->i_flags |= APPEND;
		else
			ip->i_flags &= ~APPEND;
#if 0
		if (sd_attrs & REISERFS_NOATIME_FL)
			ip->i_flags |= S_NOATIME;
		else
			ip->i_flags &= ~S_NOATIME;
		if (sd_attrs & REISERFS_NOTAIL_FL)
			REISERFS_I(ip)->i_flags |= i_nopack_mask;
		else
			REISERFS_I(ip)->i_flags &= ~i_nopack_mask;
#endif
	}
}

void
i_attrs_to_sd_attrs(struct reiserfs_node *ip, uint16_t *sd_attrs)
{

	if (reiserfs_attrs(ip->i_reiserfs)) {
#if 0
		if (ip->i_flags & S_SYNC)
			*sd_attrs |= REISERFS_SYNC_FL;
		else
			*sd_attrs &= ~REISERFS_SYNC_FL;
#endif
		if (ip->i_flags & IMMUTABLE)
			*sd_attrs |= REISERFS_IMMUTABLE_FL;
		else
			*sd_attrs &= ~REISERFS_IMMUTABLE_FL;
		if (ip->i_flags & APPEND)
			*sd_attrs |= REISERFS_APPEND_FL;
		else
			*sd_attrs &= ~REISERFS_APPEND_FL;
#if 0
		if (ip->i_flags & S_NOATIME)
			*sd_attrs |= REISERFS_NOATIME_FL;
		else
			*sd_attrs &= ~REISERFS_NOATIME_FL;
		if (REISERFS_I(ip)->i_flags & i_nopack_mask)
			*sd_attrs |= REISERFS_NOTAIL_FL;
		else
			*sd_attrs &= ~REISERFS_NOTAIL_FL;
#endif
	}
}
