/*
 * inode.c
 *
 * PURPOSE
 *  Inode handling routines for the OSTA-UDF(tm) filesystem.
 *
 * CONTACTS
 *  E-mail regarding any portion of the Linux UDF file system should be
 *  directed to the development team mailing list (run by majordomo):
 *    linux_udf@hpesjro.fc.hp.com
 *
 * COPYRIGHT
 *  This file is distributed under the terms of the GNU General Public
 *  License (GPL). Copies of the GPL can be obtained from:
 *    ftp://prep.ai.mit.edu/pub/gnu/GPL
 *  Each contributing author retains all rights to their own work.
 *
 *  (C) 1998 Dave Boynton
 *  (C) 1998-2001 Ben Fennema
 *  (C) 1999-2000 Stelias Computing Inc
 *
 * HISTORY
 *
 *  10/04/98 dgb  Added rudimentary directory functions
 *  10/07/98      Fully working udf_block_map! It works!
 *  11/25/98      bmap altered to better support extents
 *  12/06/98 blf  partition support in udf_iget, udf_block_map and udf_read_inode
 *  12/12/98      rewrote udf_block_map to handle next extents and descs across
 *                block boundaries (which is not actually allowed)
 *  12/20/98      added support for strategy 4096
 *  03/07/99      rewrote udf_block_map (again)
 *                New funcs, inode_bmap, udf_next_aext
 *  04/19/99      Support for writing device EA's for major/minor #
 */

#include "udfdecl.h"
#include <linux/locks.h>
#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/module.h>

#include "udf_i.h"
#include "udf_sb.h"

MODULE_AUTHOR("Ben Fennema");
MODULE_DESCRIPTION("Universal Disk Format Filesystem");
MODULE_LICENSE("GPL");

#define EXTENT_MERGE_SIZE 5

static mode_t udf_convert_permissions(struct fileEntry *);
static int udf_update_inode(struct inode *, int);
static void udf_fill_inode(struct inode *, struct buffer_head *);
static struct buffer_head *inode_getblk(struct inode *, long, int *, long *, int *);
static void udf_split_extents(struct inode *, int *, int, int,
	long_ad [EXTENT_MERGE_SIZE], int *);
static void udf_prealloc_extents(struct inode *, int, int,
	 long_ad [EXTENT_MERGE_SIZE], int *);
static void udf_merge_extents(struct inode *,
	 long_ad [EXTENT_MERGE_SIZE], int *);
static void udf_update_extents(struct inode *,
	long_ad [EXTENT_MERGE_SIZE], int, int,
	lb_addr, uint32_t, struct buffer_head **);
static int udf_get_block(struct inode *, long, struct buffer_head *, int);

/*
 * udf_put_inode
 *
 * PURPOSE
 *
 * DESCRIPTION
 *	This routine is called whenever the kernel no longer needs the inode.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 *  Called at each iput()
 */
void udf_put_inode(struct inode * inode)
{
	if (!(inode->i_sb->s_flags & MS_RDONLY))
	{
		lock_kernel();
		udf_discard_prealloc(inode);
		unlock_kernel();
	}
}

/*
 * udf_delete_inode
 *
 * PURPOSE
 *	Clean-up before the specified inode is destroyed.
 *
 * DESCRIPTION
 *	This routine is called when the kernel destroys an inode structure
 *	ie. when iput() finds i_count == 0.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 *  Called at the last iput() if i_nlink is zero.
 */
void udf_delete_inode(struct inode * inode)
{
	lock_kernel();

	if (is_bad_inode(inode))
		goto no_delete;

	inode->i_size = 0;
	udf_truncate(inode);
	udf_update_inode(inode, IS_SYNC(inode));
	udf_free_inode(inode);

	unlock_kernel();
	return;
no_delete:
	unlock_kernel();
	clear_inode(inode);
}

void udf_discard_prealloc(struct inode * inode)
{
	if (inode->i_size && inode->i_size != UDF_I_LENEXTENTS(inode) &&
		UDF_I_ALLOCTYPE(inode) != ICBTAG_FLAG_AD_IN_ICB)
	{
		udf_truncate_extents(inode);
	}
}

static int udf_writepage(struct page *page)
{
	return block_write_full_page(page, udf_get_block);
}

static int udf_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, udf_get_block);
}

static int udf_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return block_prepare_write(page, from, to, udf_get_block);
}

static int udf_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping,block,udf_get_block);
}

struct address_space_operations udf_aops = {
	readpage:		udf_readpage,
	writepage:		udf_writepage,
	sync_page:		block_sync_page,
	prepare_write:		udf_prepare_write,
	commit_write:		generic_commit_write,
	bmap:			udf_bmap,
};

void udf_expand_file_adinicb(struct inode * inode, int newsize, int * err)
{
	struct buffer_head *bh = NULL;
	struct page *page;
	char *kaddr;
	int block;

	/* from now on we have normal address_space methods */
	inode->i_data.a_ops = &udf_aops;

	if (!UDF_I_LENALLOC(inode))
	{
		if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
			UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_SHORT;
		else
			UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_LONG;
		mark_inode_dirty(inode);
		return;
	}

	block = udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0);
	bh = udf_tread(inode->i_sb, block);
	if (!bh)
		return;
	page = grab_cache_page(inode->i_mapping, 0);
	if (!PageLocked(page))
		PAGE_BUG(page);
	if (!Page_Uptodate(page))
	{
		kaddr = kmap(page);
		memset(kaddr + UDF_I_LENALLOC(inode), 0x00,
			PAGE_CACHE_SIZE - UDF_I_LENALLOC(inode));
		memcpy(kaddr, bh->b_data + udf_file_entry_alloc_offset(inode),
			UDF_I_LENALLOC(inode));
		flush_dcache_page(page);
		SetPageUptodate(page);
		kunmap(page);
	}
	memset(bh->b_data + udf_file_entry_alloc_offset(inode),
		0, UDF_I_LENALLOC(inode));
	UDF_I_LENALLOC(inode) = 0;
	if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_SHORT;
	else
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_LONG;
	mark_buffer_dirty_inode(bh, inode);
	udf_release_data(bh);

	inode->i_data.a_ops->writepage(page);
	page_cache_release(page);

	mark_inode_dirty(inode);
	inode->i_version ++;
}

struct buffer_head * udf_expand_dir_adinicb(struct inode *inode, int *block, int *err)
{
	int newblock;
	struct buffer_head *sbh = NULL, *dbh = NULL;
	lb_addr bloc, eloc;
	uint32_t elen, extoffset;

	struct udf_fileident_bh sfibh, dfibh;
	loff_t f_pos = udf_ext0_offset(inode) >> 2;
	int size = (udf_ext0_offset(inode) + inode->i_size) >> 2;
	struct fileIdentDesc cfi, *sfi, *dfi;

	if (!inode->i_size)
	{
		if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
			UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_SHORT;
		else
			UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_LONG;
		mark_inode_dirty(inode);
		return NULL;
	}

	/* alloc block, and copy data to it */
	*block = udf_new_block(inode->i_sb, inode,
		UDF_I_LOCATION(inode).partitionReferenceNum,
		UDF_I_LOCATION(inode).logicalBlockNum, err);

	if (!(*block))
		return NULL;
	newblock = udf_get_pblock(inode->i_sb, *block,
		UDF_I_LOCATION(inode).partitionReferenceNum, 0);
	if (!newblock)
		return NULL;
	sbh = udf_tread(inode->i_sb, inode->i_ino);
	if (!sbh)
		return NULL;
	dbh = udf_tgetblk(inode->i_sb, newblock);
	if (!dbh)
		return NULL;
	lock_buffer(dbh);
	memset(dbh->b_data, 0x00, inode->i_sb->s_blocksize);
	mark_buffer_uptodate(dbh, 1);
	unlock_buffer(dbh);
	mark_buffer_dirty_inode(dbh, inode);

	sfibh.soffset = sfibh.eoffset = (f_pos & ((inode->i_sb->s_blocksize - 1) >> 2)) << 2;
	sfibh.sbh = sfibh.ebh = sbh;
	dfibh.soffset = dfibh.eoffset = 0;
	dfibh.sbh = dfibh.ebh = dbh;
	while ( (f_pos < size) )
	{
		sfi = udf_fileident_read(inode, &f_pos, &sfibh, &cfi, NULL, NULL, NULL, NULL, NULL, NULL);
		if (!sfi)
		{
			udf_release_data(sbh);
			udf_release_data(dbh);
			return NULL;
		}
		sfi->descTag.tagLocation = *block;
		dfibh.soffset = dfibh.eoffset;
		dfibh.eoffset += (sfibh.eoffset - sfibh.soffset);
		dfi = (struct fileIdentDesc *)(dbh->b_data + dfibh.soffset);
		if (udf_write_fi(inode, sfi, dfi, &dfibh, sfi->impUse,
			sfi->fileIdent + sfi->lengthOfImpUse))
		{
			udf_release_data(sbh);
			udf_release_data(dbh);
			return NULL;
		}
	}
	mark_buffer_dirty_inode(dbh, inode);

	memset(sbh->b_data + udf_file_entry_alloc_offset(inode),
		0, UDF_I_LENALLOC(inode));

	UDF_I_LENALLOC(inode) = 0;
	if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_USE_SHORT_AD))
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_SHORT;
	else
		UDF_I_ALLOCTYPE(inode) = ICBTAG_FLAG_AD_LONG;
	bloc = UDF_I_LOCATION(inode);
	eloc.logicalBlockNum = *block;
	eloc.partitionReferenceNum = UDF_I_LOCATION(inode).partitionReferenceNum;
	elen = inode->i_size;
	UDF_I_LENEXTENTS(inode) = elen;
	extoffset = udf_file_entry_alloc_offset(inode);
	udf_add_aext(inode, &bloc, &extoffset, eloc, elen, &sbh, 0);
	/* UniqueID stuff */

	mark_buffer_dirty(sbh);
	udf_release_data(sbh);
	mark_inode_dirty(inode);
	inode->i_version ++;
	return dbh;
}

static int udf_get_block(struct inode *inode, long block, struct buffer_head *bh_result, int create)
{
	int err, new;
	struct buffer_head *bh;
	unsigned long phys;

	if (!create)
	{
		phys = udf_block_map(inode, block);
		if (phys)
		{
			bh_result->b_dev = inode->i_dev;
			bh_result->b_blocknr = phys;
			bh_result->b_state |= (1UL << BH_Mapped);
		}
		return 0;
	}

	err = -EIO;
	new = 0;
	bh = NULL;

	lock_kernel();

	if (block < 0)
		goto abort_negative;

	if (block == UDF_I_NEXT_ALLOC_BLOCK(inode) + 1)
	{
		UDF_I_NEXT_ALLOC_BLOCK(inode) ++;
		UDF_I_NEXT_ALLOC_GOAL(inode) ++;
	}

	err = 0;

	bh = inode_getblk(inode, block, &err, &phys, &new);
	if (bh)
		BUG();
	if (err)
		goto abort;
	if (!phys)
		BUG();

	bh_result->b_dev = inode->i_dev;
	bh_result->b_blocknr = phys;
	bh_result->b_state |= (1UL << BH_Mapped);
	if (new)
		bh_result->b_state |= (1UL << BH_New);
abort:
	unlock_kernel();
	return err;

abort_negative:
	udf_warning(inode->i_sb, "udf_get_block", "block < 0");
	goto abort;
}

struct buffer_head * udf_getblk(struct inode * inode, long block,
	int create, int * err)
{
	struct buffer_head dummy;

	dummy.b_state = 0;
	dummy.b_blocknr = -1000;
	*err = udf_get_block(inode, block, &dummy, create);
	if (!*err && buffer_mapped(&dummy))
	{
		struct buffer_head *bh;
		bh = sb_getblk(inode->i_sb, dummy.b_blocknr);
		if (buffer_new(&dummy))
		{
			lock_buffer(bh);
			memset(bh->b_data, 0x00, inode->i_sb->s_blocksize);
			mark_buffer_uptodate(bh, 1);
			unlock_buffer(bh);
			mark_buffer_dirty_inode(bh, inode);
		}
		return bh;
	}
	return NULL;
}

static struct buffer_head * inode_getblk(struct inode * inode, long block,
	int *err, long *phys, int *new)
{
	struct buffer_head *pbh = NULL, *cbh = NULL, *nbh = NULL, *result = NULL;
	long_ad laarr[EXTENT_MERGE_SIZE];
	uint32_t pextoffset = 0, cextoffset = 0, nextoffset = 0;
	int count = 0, startnum = 0, endnum = 0;
	uint32_t elen = 0;
	lb_addr eloc, pbloc, cbloc, nbloc;
	int c = 1;
	uint64_t lbcount = 0, b_off = 0;
	uint32_t newblocknum, newblock, offset = 0;
	int8_t etype;
	int goal = 0, pgoal = UDF_I_LOCATION(inode).logicalBlockNum;
	char lastblock = 0;

	pextoffset = cextoffset = nextoffset = udf_file_entry_alloc_offset(inode);
	b_off = (uint64_t)block << inode->i_sb->s_blocksize_bits;
	pbloc = cbloc = nbloc = UDF_I_LOCATION(inode);

	/* find the extent which contains the block we are looking for.
       alternate between laarr[0] and laarr[1] for locations of the
       current extent, and the previous extent */
	do
	{
		if (pbh != cbh)
		{
			udf_release_data(pbh);
			atomic_inc(&cbh->b_count);
			pbh = cbh;
		}
		if (cbh != nbh)
		{
			udf_release_data(cbh);
			atomic_inc(&nbh->b_count);
			cbh = nbh;
		}

		lbcount += elen;

		pbloc = cbloc;
		cbloc = nbloc;

		pextoffset = cextoffset;
		cextoffset = nextoffset;

		if ((etype = udf_next_aext(inode, &nbloc, &nextoffset, &eloc, &elen, &nbh, 1)) == -1)
			break;

		c = !c;

		laarr[c].extLength = (etype << 30) | elen;
		laarr[c].extLocation = eloc;

		if (etype != (EXT_NOT_RECORDED_NOT_ALLOCATED >> 30))
			pgoal = eloc.logicalBlockNum +
				((elen + inode->i_sb->s_blocksize - 1) >>
				inode->i_sb->s_blocksize_bits);

		count ++;
	} while (lbcount + elen <= b_off);

	b_off -= lbcount;
	offset = b_off >> inode->i_sb->s_blocksize_bits;

	/* if the extent is allocated and recorded, return the block
       if the extent is not a multiple of the blocksize, round up */

	if (etype == (EXT_RECORDED_ALLOCATED >> 30))
	{
		if (elen & (inode->i_sb->s_blocksize - 1))
		{
			elen = EXT_RECORDED_ALLOCATED |
				((elen + inode->i_sb->s_blocksize - 1) &
				~(inode->i_sb->s_blocksize - 1));
			etype = udf_write_aext(inode, nbloc, &cextoffset, eloc, elen, nbh, 1);
		}
		udf_release_data(pbh);
		udf_release_data(cbh);
		udf_release_data(nbh);
		newblock = udf_get_lb_pblock(inode->i_sb, eloc, offset);
		*phys = newblock;
		return NULL;
	}

	if (etype == -1)
	{
		endnum = startnum = ((count > 1) ? 1 : count);
		if (laarr[c].extLength & (inode->i_sb->s_blocksize - 1))
		{
			laarr[c].extLength =
				(laarr[c].extLength & UDF_EXTENT_FLAG_MASK) |
				(((laarr[c].extLength & UDF_EXTENT_LENGTH_MASK) +
					inode->i_sb->s_blocksize - 1) &
				~(inode->i_sb->s_blocksize - 1));
			UDF_I_LENEXTENTS(inode) =
				(UDF_I_LENEXTENTS(inode) + inode->i_sb->s_blocksize - 1) &
					~(inode->i_sb->s_blocksize - 1);
		}
		c = !c;
		laarr[c].extLength = EXT_NOT_RECORDED_NOT_ALLOCATED |
			((offset + 1) << inode->i_sb->s_blocksize_bits);
		memset(&laarr[c].extLocation, 0x00, sizeof(lb_addr));
		count ++;
		endnum ++;
		lastblock = 1;
	}
	else
		endnum = startnum = ((count > 2) ? 2 : count);

	/* if the current extent is in position 0, swap it with the previous */
	if (!c && count != 1)
	{
		laarr[2] = laarr[0];
		laarr[0] = laarr[1];
		laarr[1] = laarr[2];
		c = 1;
	}

	/* if the current block is located in a extent, read the next extent */
	if (etype != -1)
	{
		if ((etype = udf_next_aext(inode, &nbloc, &nextoffset, &eloc, &elen, &nbh, 0)) != -1)
		{
			laarr[c+1].extLength = (etype << 30) | elen;
			laarr[c+1].extLocation = eloc;
			count ++;
			startnum ++;
			endnum ++;
		}
		else
			lastblock = 1;
	}
	udf_release_data(nbh);
	if (!pbh)
		pbh = cbh;
	else
		udf_release_data(cbh);

	/* if the current extent is not recorded but allocated, get the
		block in the extent corresponding to the requested block */
	if ((laarr[c].extLength >> 30) == (EXT_NOT_RECORDED_ALLOCATED >> 30))
		newblocknum = laarr[c].extLocation.logicalBlockNum + offset;
	else /* otherwise, allocate a new block */
	{
		if (UDF_I_NEXT_ALLOC_BLOCK(inode) == block)
			goal = UDF_I_NEXT_ALLOC_GOAL(inode);

		if (!goal)
		{
			if (!(goal = pgoal))
				goal = UDF_I_LOCATION(inode).logicalBlockNum + 1;
		}

		if (!(newblocknum = udf_new_block(inode->i_sb, inode,
			UDF_I_LOCATION(inode).partitionReferenceNum, goal, err)))
		{
			udf_release_data(pbh);
			*err = -ENOSPC;
			return NULL;
		}
		UDF_I_LENEXTENTS(inode) += inode->i_sb->s_blocksize;
	}

	/* if the extent the requsted block is located in contains multiple blocks,
       split the extent into at most three extents. blocks prior to requested
       block, requested block, and blocks after requested block */
	udf_split_extents(inode, &c, offset, newblocknum, laarr, &endnum);

#ifdef UDF_PREALLOCATE
	/* preallocate blocks */
	udf_prealloc_extents(inode, c, lastblock, laarr, &endnum);
#endif

	/* merge any continuous blocks in laarr */
	udf_merge_extents(inode, laarr, &endnum);

	/* write back the new extents, inserting new extents if the new number
       of extents is greater than the old number, and deleting extents if
       the new number of extents is less than the old number */
	udf_update_extents(inode, laarr, startnum, endnum, pbloc, pextoffset, &pbh);

	udf_release_data(pbh);

	if (!(newblock = udf_get_pblock(inode->i_sb, newblocknum,
		UDF_I_LOCATION(inode).partitionReferenceNum, 0)))
	{
		return NULL;
	}
	*phys = newblock;
	*err = 0;
	*new = 1;
	UDF_I_NEXT_ALLOC_BLOCK(inode) = block;
	UDF_I_NEXT_ALLOC_GOAL(inode) = newblocknum;
	inode->i_ctime = CURRENT_TIME;
	UDF_I_UCTIME(inode) = CURRENT_UTIME;

	if (IS_SYNC(inode))
		udf_sync_inode(inode);
	else
		mark_inode_dirty(inode);
	return result;
}

static void udf_split_extents(struct inode *inode, int *c, int offset, int newblocknum,
	long_ad laarr[EXTENT_MERGE_SIZE], int *endnum)
{
	if ((laarr[*c].extLength >> 30) == (EXT_NOT_RECORDED_ALLOCATED >> 30) ||
		(laarr[*c].extLength >> 30) == (EXT_NOT_RECORDED_NOT_ALLOCATED >> 30))
	{
		int curr = *c;
		int blen = ((laarr[curr].extLength & UDF_EXTENT_LENGTH_MASK) +
			inode->i_sb->s_blocksize - 1) >> inode->i_sb->s_blocksize_bits;
		int type = laarr[curr].extLength & ~UDF_EXTENT_LENGTH_MASK;

		if (blen == 1)
			;
		else if (!offset || blen == offset + 1)
		{
			laarr[curr+2] = laarr[curr+1];
			laarr[curr+1] = laarr[curr];
		}
		else
		{
			laarr[curr+3] = laarr[curr+1];
			laarr[curr+2] = laarr[curr+1] = laarr[curr];
		}

		if (offset)
		{
			if ((type >> 30) == (EXT_NOT_RECORDED_ALLOCATED >> 30))
			{
				udf_free_blocks(inode->i_sb, inode, laarr[curr].extLocation, 0, offset);
				laarr[curr].extLength = EXT_NOT_RECORDED_NOT_ALLOCATED |
					(offset << inode->i_sb->s_blocksize_bits);
				laarr[curr].extLocation.logicalBlockNum = 0;
				laarr[curr].extLocation.partitionReferenceNum = 0;
			}
			else
				laarr[curr].extLength = type |
					(offset << inode->i_sb->s_blocksize_bits);
			curr ++;
			(*c) ++;
			(*endnum) ++;
		}
		
		laarr[curr].extLocation.logicalBlockNum = newblocknum;
		if ((type >> 30) == (EXT_NOT_RECORDED_NOT_ALLOCATED >> 30))
			laarr[curr].extLocation.partitionReferenceNum =
				UDF_I_LOCATION(inode).partitionReferenceNum;
		laarr[curr].extLength = EXT_RECORDED_ALLOCATED |
			inode->i_sb->s_blocksize;
		curr ++;

		if (blen != offset + 1)
		{
			if ((type >> 30) == (EXT_NOT_RECORDED_ALLOCATED >> 30))
				laarr[curr].extLocation.logicalBlockNum += (offset + 1);
			laarr[curr].extLength = type |
				((blen - (offset + 1)) << inode->i_sb->s_blocksize_bits);
			curr ++;
			(*endnum) ++;
		}
	}
}

static void udf_prealloc_extents(struct inode *inode, int c, int lastblock,
	 long_ad laarr[EXTENT_MERGE_SIZE], int *endnum)
{
	int start, length = 0, currlength = 0, i;

	if (*endnum >= (c+1))
	{
		if (!lastblock)
			return;
		else
			start = c;
	}
	else
	{
		if ((laarr[c+1].extLength >> 30) == (EXT_NOT_RECORDED_ALLOCATED >> 30))
		{
			start = c+1;
			length = currlength = (((laarr[c+1].extLength & UDF_EXTENT_LENGTH_MASK) +
				inode->i_sb->s_blocksize - 1) >> inode->i_sb->s_blocksize_bits);
		}
		else
			start = c;
	}

	for (i=start+1; i<=*endnum; i++)
	{
		if (i == *endnum)
		{
			if (lastblock)
				length += UDF_DEFAULT_PREALLOC_BLOCKS;
		}
		else if ((laarr[i].extLength >> 30) == (EXT_NOT_RECORDED_NOT_ALLOCATED >> 30))
			length += (((laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) +
				inode->i_sb->s_blocksize - 1) >> inode->i_sb->s_blocksize_bits);
		else
			break;
	}

	if (length)
	{
		int next = laarr[start].extLocation.logicalBlockNum +
			(((laarr[start].extLength & UDF_EXTENT_LENGTH_MASK) +
			inode->i_sb->s_blocksize - 1) >> inode->i_sb->s_blocksize_bits);
		int numalloc = udf_prealloc_blocks(inode->i_sb, inode,
			laarr[start].extLocation.partitionReferenceNum,
			next, (UDF_DEFAULT_PREALLOC_BLOCKS > length ? length :
				UDF_DEFAULT_PREALLOC_BLOCKS) - currlength);

		if (numalloc)
		{
			if (start == (c+1))
				laarr[start].extLength +=
					(numalloc << inode->i_sb->s_blocksize_bits);
			else
			{
				memmove(&laarr[c+2], &laarr[c+1],
					sizeof(long_ad) * (*endnum - (c+1)));
				(*endnum) ++;
				laarr[c+1].extLocation.logicalBlockNum = next;
				laarr[c+1].extLocation.partitionReferenceNum =
					laarr[c].extLocation.partitionReferenceNum;
				laarr[c+1].extLength = EXT_NOT_RECORDED_ALLOCATED |
					(numalloc << inode->i_sb->s_blocksize_bits);
				start = c+1;
			}

			for (i=start+1; numalloc && i<*endnum; i++)
			{
				int elen = ((laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) +
					inode->i_sb->s_blocksize - 1) >> inode->i_sb->s_blocksize_bits;

				if (elen > numalloc)
				{
					laarr[c].extLength -=
						(numalloc << inode->i_sb->s_blocksize_bits);
					numalloc = 0;
				}
				else
				{
					numalloc -= elen;
					if (*endnum > (i+1))
						memmove(&laarr[i], &laarr[i+1], 
							sizeof(long_ad) * (*endnum - (i+1)));
					i --;
					(*endnum) --;
				}
			}
			UDF_I_LENEXTENTS(inode) += numalloc << inode->i_sb->s_blocksize_bits;
		}
	}
}

static void udf_merge_extents(struct inode *inode,
	 long_ad laarr[EXTENT_MERGE_SIZE], int *endnum)
{
	int i;

	for (i=0; i<(*endnum-1); i++)
	{
		if ((laarr[i].extLength >> 30) == (laarr[i+1].extLength >> 30))
		{
			if (((laarr[i].extLength >> 30) == (EXT_NOT_RECORDED_NOT_ALLOCATED >> 30)) ||
				((laarr[i+1].extLocation.logicalBlockNum - laarr[i].extLocation.logicalBlockNum) ==
				(((laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) +
				inode->i_sb->s_blocksize - 1) >> inode->i_sb->s_blocksize_bits)))
			{
				if (((laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) +
					(laarr[i+1].extLength & UDF_EXTENT_LENGTH_MASK) +
					inode->i_sb->s_blocksize - 1) & ~UDF_EXTENT_LENGTH_MASK)
				{
					laarr[i+1].extLength = (laarr[i+1].extLength -
						(laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) +
						UDF_EXTENT_LENGTH_MASK) & ~(inode->i_sb->s_blocksize-1);
					laarr[i].extLength = (UDF_EXTENT_LENGTH_MASK + 1) -
						inode->i_sb->s_blocksize;
					laarr[i+1].extLocation.logicalBlockNum =
						laarr[i].extLocation.logicalBlockNum +
						((laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) >>
							inode->i_sb->s_blocksize_bits);
				}
				else
				{
					laarr[i].extLength = laarr[i+1].extLength +
						(((laarr[i].extLength & UDF_EXTENT_LENGTH_MASK) +
						inode->i_sb->s_blocksize - 1) & ~(inode->i_sb->s_blocksize-1));
					if (*endnum > (i+2))
						memmove(&laarr[i+1], &laarr[i+2],
							sizeof(long_ad) * (*endnum - (i+2)));
					i --;
					(*endnum) --;
				}
			}
		}
	}
}

static void udf_update_extents(struct inode *inode,
	long_ad laarr[EXTENT_MERGE_SIZE], int startnum, int endnum,
	lb_addr pbloc, uint32_t pextoffset, struct buffer_head **pbh)
{
	int start = 0, i;
	lb_addr tmploc;
	uint32_t tmplen;

	if (startnum > endnum)
	{
		for (i=0; i<(startnum-endnum); i++)
		{
			udf_delete_aext(inode, pbloc, pextoffset, laarr[i].extLocation,
				laarr[i].extLength, *pbh);
		}
	}
	else if (startnum < endnum)
	{
		for (i=0; i<(endnum-startnum); i++)
		{
			udf_insert_aext(inode, pbloc, pextoffset, laarr[i].extLocation,
				laarr[i].extLength, *pbh);
			udf_next_aext(inode, &pbloc, &pextoffset, &laarr[i].extLocation,
				&laarr[i].extLength, pbh, 1);
			start ++;
		}
	}

	for (i=start; i<endnum; i++)
	{
		udf_next_aext(inode, &pbloc, &pextoffset, &tmploc, &tmplen, pbh, 0);
		udf_write_aext(inode, pbloc, &pextoffset, laarr[i].extLocation,
			laarr[i].extLength, *pbh, 1);
	}
}

struct buffer_head * udf_bread(struct inode * inode, int block,
	int create, int * err)
{
	struct buffer_head * bh = NULL;

	bh = udf_getblk(inode, block, create, err);
	if (!bh)
		return NULL;

	if (buffer_uptodate(bh))
		return bh;
	ll_rw_block(READ, 1, &bh);
	wait_on_buffer(bh);
	if (buffer_uptodate(bh))
		return bh;
	brelse(bh);
	*err = -EIO;
	return NULL;
}

void udf_truncate(struct inode * inode)
{
	int offset;
	struct buffer_head *bh;
	int err;

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
			S_ISLNK(inode->i_mode)))
		return;
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return;

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
	{
		if (inode->i_sb->s_blocksize < (udf_file_entry_alloc_offset(inode) +
			inode->i_size))
		{
			udf_expand_file_adinicb(inode, inode->i_size, &err);
			if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
			{
				inode->i_size = UDF_I_LENALLOC(inode);
				return;
			}
			else
				udf_truncate_extents(inode);
		}
		else
		{
			offset = (inode->i_size & (inode->i_sb->s_blocksize - 1)) +
				udf_file_entry_alloc_offset(inode);

			if ((bh = udf_tread(inode->i_sb,
				udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0))))
			{
				memset(bh->b_data + offset, 0x00, inode->i_sb->s_blocksize - offset);
				mark_buffer_dirty(bh);
				udf_release_data(bh);
			}
			UDF_I_LENALLOC(inode) = inode->i_size;
		}
	}
	else
	{
		block_truncate_page(inode->i_mapping, inode->i_size, udf_get_block);
		udf_truncate_extents(inode);
	}	

	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	UDF_I_UMTIME(inode) = UDF_I_UCTIME(inode) = CURRENT_UTIME;
	if (IS_SYNC(inode))
		udf_sync_inode (inode);
	else
		mark_inode_dirty(inode);
}

/*
 * udf_read_inode
 *
 * PURPOSE
 *	Read an inode.
 *
 * DESCRIPTION
 *	This routine is called by iget() [which is called by udf_iget()]
 *      (clean_inode() will have been called first)
 *	when an inode is first read into memory.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 * 12/19/98 dgb  Updated to fix size problems.
 */

void
udf_read_inode(struct inode *inode)
{
	memset(&UDF_I_LOCATION(inode), 0xFF, sizeof(lb_addr));
}

void
__udf_read_inode(struct inode *inode)
{
	struct buffer_head *bh = NULL;
	struct fileEntry *fe;
	uint16_t ident;

	/*
	 * Set defaults, but the inode is still incomplete!
	 * Note: get_new_inode() sets the following on a new inode:
	 *      i_sb = sb
	 *      i_dev = sb->s_dev;
	 *      i_no = ino
	 *      i_flags = sb->s_flags
	 *      i_state = 0
	 * clean_inode(): zero fills and sets
	 *      i_count = 1
	 *      i_nlink = 1
	 *      i_op = NULL;
	 */

	inode->i_blksize = PAGE_SIZE;

	bh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 0, &ident);

	if (!bh)
	{
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) failed !bh\n",
			inode->i_ino);
		make_bad_inode(inode);
		return;
	}

	if (ident != TAG_IDENT_FE && ident != TAG_IDENT_EFE &&
		ident != TAG_IDENT_USE)
	{
		printk(KERN_ERR "udf: udf_read_inode(ino %ld) failed ident=%d\n",
			inode->i_ino, ident);
		udf_release_data(bh);
		make_bad_inode(inode);
		return;
	}

	fe = (struct fileEntry *)bh->b_data;

	if (le16_to_cpu(fe->icbTag.strategyType) == 4096)
	{
		struct buffer_head *ibh = NULL, *nbh = NULL;
		struct indirectEntry *ie;

		ibh = udf_read_ptagged(inode->i_sb, UDF_I_LOCATION(inode), 1, &ident);
		if (ident == TAG_IDENT_IE)
		{
			if (ibh)
			{
				lb_addr loc;
				ie = (struct indirectEntry *)ibh->b_data;
	
				loc = lelb_to_cpu(ie->indirectICB.extLocation);
	
				if (ie->indirectICB.extLength && 
					(nbh = udf_read_ptagged(inode->i_sb, loc, 0, &ident)))
				{
					if (ident == TAG_IDENT_FE ||
						ident == TAG_IDENT_EFE)
					{
						memcpy(&UDF_I_LOCATION(inode), &loc, sizeof(lb_addr));
						udf_release_data(bh);
						udf_release_data(ibh);
						udf_release_data(nbh);
						__udf_read_inode(inode);
						return;
					}
					else
					{
						udf_release_data(nbh);
						udf_release_data(ibh);
					}
				}
				else
					udf_release_data(ibh);
			}
		}
		else
			udf_release_data(ibh);
	}
	else if (le16_to_cpu(fe->icbTag.strategyType) != 4)
	{
		printk(KERN_ERR "udf: unsupported strategy type: %d\n",
			le16_to_cpu(fe->icbTag.strategyType));
		udf_release_data(bh);
		make_bad_inode(inode);
		return;
	}
	udf_fill_inode(inode, bh);
	udf_release_data(bh);
}

static void udf_fill_inode(struct inode *inode, struct buffer_head *bh)
{
	struct fileEntry *fe;
	struct extendedFileEntry *efe;
	time_t convtime;
	long convtime_usec;
	int offset, alen;

	inode->i_version = ++event;
	UDF_I_NEW_INODE(inode) = 0;

	fe = (struct fileEntry *)bh->b_data;
	efe = (struct extendedFileEntry *)bh->b_data;

	if (le16_to_cpu(fe->icbTag.strategyType) == 4)
		UDF_I_STRAT4096(inode) = 0;
	else /* if (le16_to_cpu(fe->icbTag.strategyType) == 4096) */
		UDF_I_STRAT4096(inode) = 1;

	UDF_I_ALLOCTYPE(inode) = le16_to_cpu(fe->icbTag.flags) & ICBTAG_FLAG_AD_MASK;
	if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_EFE)
		UDF_I_EXTENDED_FE(inode) = 1;
	else if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_FE)
		UDF_I_EXTENDED_FE(inode) = 0;
	else if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_USE)
	{
		UDF_I_LENALLOC(inode) =
			le32_to_cpu(
				((struct unallocSpaceEntry *)bh->b_data)->lengthAllocDescs);
		return;
	}

	inode->i_uid = le32_to_cpu(fe->uid);
	if ( inode->i_uid == -1 ) inode->i_uid = UDF_SB(inode->i_sb)->s_uid;

	inode->i_gid = le32_to_cpu(fe->gid);
	if ( inode->i_gid == -1 ) inode->i_gid = UDF_SB(inode->i_sb)->s_gid;

	inode->i_nlink = le16_to_cpu(fe->fileLinkCount);
	if (!inode->i_nlink)
		inode->i_nlink = 1;
	
	inode->i_size = le64_to_cpu(fe->informationLength);
	UDF_I_LENEXTENTS(inode) = inode->i_size;

	inode->i_mode = udf_convert_permissions(fe);
	inode->i_mode &= ~UDF_SB(inode->i_sb)->s_umask;

	UDF_I_NEXT_ALLOC_BLOCK(inode) = 0;
	UDF_I_NEXT_ALLOC_GOAL(inode) = 0;

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		inode->i_blocks = le64_to_cpu(fe->logicalBlocksRecorded) <<
			(inode->i_sb->s_blocksize_bits - 9);

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(fe->accessTime)) )
		{
			inode->i_atime = convtime;
		}
		else
		{
			inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);
		}

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(fe->modificationTime)) )
		{
			inode->i_mtime = convtime;
			UDF_I_UMTIME(inode) = convtime_usec;
		}
		else
		{
			inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);
			UDF_I_UMTIME(inode) = 0;
		}

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(fe->attrTime)) )
		{
			inode->i_ctime = convtime;
			UDF_I_UCTIME(inode) = convtime_usec;
		}
		else
		{
			inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);
			UDF_I_UCTIME(inode) = 0;
		}

		UDF_I_UNIQUE(inode) = le64_to_cpu(fe->uniqueID);
		UDF_I_LENEATTR(inode) = le32_to_cpu(fe->lengthExtendedAttr);
		UDF_I_LENALLOC(inode) = le32_to_cpu(fe->lengthAllocDescs);
		offset = sizeof(struct fileEntry) + UDF_I_LENEATTR(inode);
		alen = offset + UDF_I_LENALLOC(inode);
	}
	else
	{
		inode->i_blocks = le64_to_cpu(efe->logicalBlocksRecorded) << 
			(inode->i_sb->s_blocksize_bits - 9);

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(efe->accessTime)) )
		{
			inode->i_atime = convtime;
		}
		else
		{
			inode->i_atime = UDF_SB_RECORDTIME(inode->i_sb);
		}

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(efe->modificationTime)) )
		{
			inode->i_mtime = convtime;
			UDF_I_UMTIME(inode) = convtime_usec;
		}
		else
		{
			inode->i_mtime = UDF_SB_RECORDTIME(inode->i_sb);
			UDF_I_UMTIME(inode) = 0;
		}

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(efe->createTime)) )
		{
			UDF_I_CRTIME(inode) = convtime;
			UDF_I_UCRTIME(inode) = convtime_usec;
		}
		else
		{
			UDF_I_CRTIME(inode) = UDF_SB_RECORDTIME(inode->i_sb);
			UDF_I_UCRTIME(inode) = 0;
		}

		if ( udf_stamp_to_time(&convtime, &convtime_usec,
			lets_to_cpu(efe->attrTime)) )
		{
			inode->i_ctime = convtime;
			UDF_I_UCTIME(inode) = convtime_usec;
		}
		else
		{
			inode->i_ctime = UDF_SB_RECORDTIME(inode->i_sb);
			UDF_I_UCTIME(inode) = 0;
		}

		UDF_I_UNIQUE(inode) = le64_to_cpu(efe->uniqueID);
		UDF_I_LENEATTR(inode) = le32_to_cpu(efe->lengthExtendedAttr);
		UDF_I_LENALLOC(inode) = le32_to_cpu(efe->lengthAllocDescs);
		offset = sizeof(struct extendedFileEntry) + UDF_I_LENEATTR(inode);
		alen = offset + UDF_I_LENALLOC(inode);
	}

	switch (fe->icbTag.fileType)
	{
		case ICBTAG_FILE_TYPE_DIRECTORY:
		{
			inode->i_op = &udf_dir_inode_operations;
			inode->i_fop = &udf_dir_operations;
			inode->i_mode |= S_IFDIR;
			inode->i_nlink ++;
			break;
		}
		case ICBTAG_FILE_TYPE_REALTIME:
		case ICBTAG_FILE_TYPE_REGULAR:
		case ICBTAG_FILE_TYPE_UNDEF:
		{
			if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_IN_ICB)
				inode->i_data.a_ops = &udf_adinicb_aops;
			else
				inode->i_data.a_ops = &udf_aops;
			inode->i_op = &udf_file_inode_operations;
			inode->i_fop = &udf_file_operations;
			inode->i_mode |= S_IFREG;
			break;
		}
		case ICBTAG_FILE_TYPE_BLOCK:
		{
			inode->i_mode |= S_IFBLK;
			break;
		}
		case ICBTAG_FILE_TYPE_CHAR:
		{
			inode->i_mode |= S_IFCHR;
			break;
		}
		case ICBTAG_FILE_TYPE_FIFO:
		{
			init_special_inode(inode, inode->i_mode | S_IFIFO, 0);
			break;
		}
		case ICBTAG_FILE_TYPE_SYMLINK:
		{
			inode->i_data.a_ops = &udf_symlink_aops;
			inode->i_op = &page_symlink_inode_operations;
			inode->i_mode = S_IFLNK|S_IRWXUGO;
			break;
		}
		default:
		{
			printk(KERN_ERR "udf: udf_fill_inode(ino %ld) failed unknown file type=%d\n",
				inode->i_ino, fe->icbTag.fileType);
			make_bad_inode(inode);
			return;
		}
	}
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
	{
		struct buffer_head *tbh = NULL;
		struct deviceSpec *dsea =
			(struct deviceSpec *)
				udf_get_extendedattr(inode, 12, 1, &tbh);

		if (dsea)
		{
			init_special_inode(inode, inode->i_mode,
				((le32_to_cpu(dsea->majorDeviceIdent)) << 8) |
				(le32_to_cpu(dsea->minorDeviceIdent) & 0xFF));
			/* Developer ID ??? */
			udf_release_data(tbh);
		}
		else
		{
			make_bad_inode(inode);
		}
	}
}

static mode_t
udf_convert_permissions(struct fileEntry *fe)
{
	mode_t mode;
	uint32_t permissions;
	uint32_t flags;

	permissions = le32_to_cpu(fe->permissions);
	flags = le16_to_cpu(fe->icbTag.flags);

	mode =	(( permissions      ) & S_IRWXO) |
		(( permissions >> 2 ) & S_IRWXG) |
		(( permissions >> 4 ) & S_IRWXU) |
		(( flags & ICBTAG_FLAG_SETUID) ? S_ISUID : 0) |
		(( flags & ICBTAG_FLAG_SETGID) ? S_ISGID : 0) |
		(( flags & ICBTAG_FLAG_STICKY) ? S_ISVTX : 0);

	return mode;
}

/*
 * udf_write_inode
 *
 * PURPOSE
 *	Write out the specified inode.
 *
 * DESCRIPTION
 *	This routine is called whenever an inode is synced.
 *	Currently this routine is just a placeholder.
 *
 * HISTORY
 *	July 1, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 */

void udf_write_inode(struct inode * inode, int sync)
{
	lock_kernel();
	udf_update_inode(inode, sync);
	unlock_kernel();
}

int udf_sync_inode(struct inode * inode)
{
	return udf_update_inode(inode, 1);
}

static int
udf_update_inode(struct inode *inode, int do_sync)
{
	struct buffer_head *bh = NULL;
	struct fileEntry *fe;
	struct extendedFileEntry *efe;
	uint32_t udfperms;
	uint16_t icbflags;
	uint16_t crclen;
	int i;
	timestamp cpu_time;
	int err = 0;

	bh = udf_tread(inode->i_sb,
		udf_get_lb_pblock(inode->i_sb, UDF_I_LOCATION(inode), 0));

	if (!bh)
	{
		udf_debug("bread failure\n");
		return -EIO;
	}
	fe = (struct fileEntry *)bh->b_data;
	efe = (struct extendedFileEntry *)bh->b_data;
	if (UDF_I_NEW_INODE(inode) == 1)
	{
		if (UDF_I_EXTENDED_FE(inode) == 0)
			memset(bh->b_data, 0x00, sizeof(struct fileEntry));
		else
			memset(bh->b_data, 0x00, sizeof(struct extendedFileEntry));
		memset(bh->b_data + udf_file_entry_alloc_offset(inode) +
			UDF_I_LENALLOC(inode), 0x0, inode->i_sb->s_blocksize -
			udf_file_entry_alloc_offset(inode) - UDF_I_LENALLOC(inode));
		UDF_I_NEW_INODE(inode) = 0;
	}

	if (le16_to_cpu(fe->descTag.tagIdent) == TAG_IDENT_USE)
	{
		struct unallocSpaceEntry *use =
			(struct unallocSpaceEntry *)bh->b_data;

		use->lengthAllocDescs = cpu_to_le32(UDF_I_LENALLOC(inode));
		crclen = sizeof(struct unallocSpaceEntry) + UDF_I_LENALLOC(inode) -
			sizeof(tag);
		use->descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(inode).logicalBlockNum);
		use->descTag.descCRCLength = cpu_to_le16(crclen);
		use->descTag.descCRC = cpu_to_le16(udf_crc((char *)use + sizeof(tag), crclen, 0));

		use->descTag.tagChecksum = 0;
		for (i=0; i<16; i++)
			if (i != 4)
				use->descTag.tagChecksum += ((uint8_t *)&(use->descTag))[i];

		mark_buffer_dirty(bh);
		udf_release_data(bh);
		return err;
	}

	if (inode->i_uid != UDF_SB(inode->i_sb)->s_uid)
		fe->uid = cpu_to_le32(inode->i_uid);

	if (inode->i_gid != UDF_SB(inode->i_sb)->s_gid)
		fe->gid = cpu_to_le32(inode->i_gid);

	udfperms =	((inode->i_mode & S_IRWXO)     ) |
			((inode->i_mode & S_IRWXG) << 2) |
			((inode->i_mode & S_IRWXU) << 4);

	udfperms |=	(le32_to_cpu(fe->permissions) &
			(FE_PERM_O_DELETE | FE_PERM_O_CHATTR |
			 FE_PERM_G_DELETE | FE_PERM_G_CHATTR |
			 FE_PERM_U_DELETE | FE_PERM_U_CHATTR));
	fe->permissions = cpu_to_le32(udfperms);

	if (S_ISDIR(inode->i_mode))
		fe->fileLinkCount = cpu_to_le16(inode->i_nlink - 1);
	else
		fe->fileLinkCount = cpu_to_le16(inode->i_nlink);

	fe->informationLength = cpu_to_le64(inode->i_size);

	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
	{
		regid *eid;
		struct buffer_head *tbh = NULL;
		struct deviceSpec *dsea =
			(struct deviceSpec *)
				udf_get_extendedattr(inode, 12, 1, &tbh);	

		if (!dsea)
		{
			dsea = (struct deviceSpec *)
				udf_add_extendedattr(inode,
					sizeof(struct deviceSpec) +
					sizeof(regid), 12, 0x3, &tbh);
			dsea->attrType = 12;
			dsea->attrSubtype = 1;
			dsea->attrLength = sizeof(struct deviceSpec) +
				sizeof(regid);
			dsea->impUseLength = sizeof(regid);
		}
		eid = (regid *)dsea->impUse;
		memset(eid, 0, sizeof(regid));
		strcpy(eid->ident, UDF_ID_DEVELOPER);
		eid->identSuffix[0] = UDF_OS_CLASS_UNIX;
		eid->identSuffix[1] = UDF_OS_ID_LINUX;
		dsea->majorDeviceIdent = kdev_t_to_nr(inode->i_rdev) >> 8;
		dsea->minorDeviceIdent = kdev_t_to_nr(inode->i_rdev) & 0xFF;
		mark_buffer_dirty_inode(tbh, inode);
		udf_release_data(tbh);
	}

	if (UDF_I_EXTENDED_FE(inode) == 0)
	{
		fe->logicalBlocksRecorded = cpu_to_le64(
			(inode->i_blocks + (1 << (inode->i_sb->s_blocksize_bits - 9)) - 1) >>
			(inode->i_sb->s_blocksize_bits - 9));

		if (udf_time_to_stamp(&cpu_time, inode->i_atime, 0))
			fe->accessTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_mtime, UDF_I_UMTIME(inode)))
			fe->modificationTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_ctime, UDF_I_UCTIME(inode)))
			fe->attrTime = cpu_to_lets(cpu_time);
		memset(&(fe->impIdent), 0, sizeof(regid));
		strcpy(fe->impIdent.ident, UDF_ID_DEVELOPER);
		fe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
		fe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
		fe->uniqueID = cpu_to_le64(UDF_I_UNIQUE(inode));
		fe->lengthExtendedAttr = cpu_to_le32(UDF_I_LENEATTR(inode));
		fe->lengthAllocDescs = cpu_to_le32(UDF_I_LENALLOC(inode));
		fe->descTag.tagIdent = le16_to_cpu(TAG_IDENT_FE);
		crclen = sizeof(struct fileEntry);
	}
	else
	{
		efe->objectSize = cpu_to_le64(inode->i_size);
		efe->logicalBlocksRecorded = cpu_to_le64(
			(inode->i_blocks + (1 << (inode->i_sb->s_blocksize_bits - 9)) - 1) >>
			(inode->i_sb->s_blocksize_bits - 9));

		if (UDF_I_CRTIME(inode) >= inode->i_atime)
		{
			UDF_I_CRTIME(inode) = inode->i_atime;
			UDF_I_UCRTIME(inode) = 0;
		}
		if (UDF_I_CRTIME(inode) > inode->i_mtime ||
			(UDF_I_CRTIME(inode) == inode->i_mtime &&
			 UDF_I_UCRTIME(inode) > UDF_I_UMTIME(inode)))
		{
			UDF_I_CRTIME(inode) = inode->i_mtime;
			UDF_I_UCRTIME(inode) = UDF_I_UMTIME(inode);
		}
		if (UDF_I_CRTIME(inode) > inode->i_ctime ||
			(UDF_I_CRTIME(inode) == inode->i_ctime &&
			 UDF_I_UCRTIME(inode) > UDF_I_UCTIME(inode)))
		{
			UDF_I_CRTIME(inode) = inode->i_ctime;
			UDF_I_UCRTIME(inode) = UDF_I_UCTIME(inode);
		}

		if (udf_time_to_stamp(&cpu_time, inode->i_atime, 0))
			efe->accessTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_mtime, UDF_I_UMTIME(inode)))
			efe->modificationTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, UDF_I_CRTIME(inode), UDF_I_UCRTIME(inode)))
			efe->createTime = cpu_to_lets(cpu_time);
		if (udf_time_to_stamp(&cpu_time, inode->i_ctime, UDF_I_UCTIME(inode)))
			efe->attrTime = cpu_to_lets(cpu_time);

		memset(&(efe->impIdent), 0, sizeof(regid));
		strcpy(efe->impIdent.ident, UDF_ID_DEVELOPER);
		efe->impIdent.identSuffix[0] = UDF_OS_CLASS_UNIX;
		efe->impIdent.identSuffix[1] = UDF_OS_ID_LINUX;
		efe->uniqueID = cpu_to_le64(UDF_I_UNIQUE(inode));
		efe->lengthExtendedAttr = cpu_to_le32(UDF_I_LENEATTR(inode));
		efe->lengthAllocDescs = cpu_to_le32(UDF_I_LENALLOC(inode));
		efe->descTag.tagIdent = le16_to_cpu(TAG_IDENT_EFE);
		crclen = sizeof(struct extendedFileEntry);
	}
	if (UDF_I_STRAT4096(inode))
	{
		fe->icbTag.strategyType = cpu_to_le16(4096);
		fe->icbTag.strategyParameter = cpu_to_le16(1);
		fe->icbTag.numEntries = cpu_to_le16(2);
	}
	else
	{
		fe->icbTag.strategyType = cpu_to_le16(4);
		fe->icbTag.numEntries = cpu_to_le16(1);
	}

	if (S_ISDIR(inode->i_mode))
		fe->icbTag.fileType = ICBTAG_FILE_TYPE_DIRECTORY;
	else if (S_ISREG(inode->i_mode))
		fe->icbTag.fileType = ICBTAG_FILE_TYPE_REGULAR;
	else if (S_ISLNK(inode->i_mode))
		fe->icbTag.fileType = ICBTAG_FILE_TYPE_SYMLINK;
	else if (S_ISBLK(inode->i_mode))
		fe->icbTag.fileType = ICBTAG_FILE_TYPE_BLOCK;
	else if (S_ISCHR(inode->i_mode))
		fe->icbTag.fileType = ICBTAG_FILE_TYPE_CHAR;
	else if (S_ISFIFO(inode->i_mode))
		fe->icbTag.fileType = ICBTAG_FILE_TYPE_FIFO;

	icbflags =	UDF_I_ALLOCTYPE(inode) |
			((inode->i_mode & S_ISUID) ? ICBTAG_FLAG_SETUID : 0) |
			((inode->i_mode & S_ISGID) ? ICBTAG_FLAG_SETGID : 0) |
			((inode->i_mode & S_ISVTX) ? ICBTAG_FLAG_STICKY : 0) |
			(le16_to_cpu(fe->icbTag.flags) &
				~(ICBTAG_FLAG_AD_MASK | ICBTAG_FLAG_SETUID |
				ICBTAG_FLAG_SETGID | ICBTAG_FLAG_STICKY));

	fe->icbTag.flags = cpu_to_le16(icbflags);
	if (UDF_SB_UDFREV(inode->i_sb) >= 0x0200)
		fe->descTag.descVersion = cpu_to_le16(3);
	else
		fe->descTag.descVersion = cpu_to_le16(2);
	fe->descTag.tagSerialNum = cpu_to_le16(UDF_SB_SERIALNUM(inode->i_sb));
	fe->descTag.tagLocation = cpu_to_le32(UDF_I_LOCATION(inode).logicalBlockNum);
	crclen += UDF_I_LENEATTR(inode) + UDF_I_LENALLOC(inode) - sizeof(tag);
	fe->descTag.descCRCLength = cpu_to_le16(crclen);
	fe->descTag.descCRC = cpu_to_le16(udf_crc((char *)fe + sizeof(tag), crclen, 0));

	fe->descTag.tagChecksum = 0;
	for (i=0; i<16; i++)
		if (i != 4)
			fe->descTag.tagChecksum += ((uint8_t *)&(fe->descTag))[i];

	/* write the data blocks */
	mark_buffer_dirty(bh);
	if (do_sync)
	{
		ll_rw_block(WRITE, 1, &bh);
		wait_on_buffer(bh);
		if (buffer_req(bh) && !buffer_uptodate(bh))
		{
			printk("IO error syncing udf inode [%s:%08lx]\n",
				bdevname(inode->i_dev), inode->i_ino);
			err = -EIO;
		}
	}
	udf_release_data(bh);
	return err;
}

/*
 * udf_iget
 *
 * PURPOSE
 *	Get an inode.
 *
 * DESCRIPTION
 *	This routine replaces iget() and read_inode().
 *
 * HISTORY
 *	October 3, 1997 - Andrew E. Mileski
 *	Written, tested, and released.
 *
 * 12/19/98 dgb  Added semaphore and changed to be a wrapper of iget
 */
struct inode *
udf_iget(struct super_block *sb, lb_addr ino)
{
	struct inode *inode;
	unsigned long block;

	block = udf_get_lb_pblock(sb, ino, 0);

	/* Get the inode */

	inode = iget(sb, block);
		/* calls udf_read_inode() ! */

	if (!inode)
	{
		printk(KERN_ERR "udf: iget() failed\n");
		return NULL;
	}
	else if (is_bad_inode(inode))
	{
		iput(inode);
		return NULL;
	}
	else if (UDF_I_LOCATION(inode).logicalBlockNum == 0xFFFFFFFF &&
		UDF_I_LOCATION(inode).partitionReferenceNum == 0xFFFF)
	{
		memcpy(&UDF_I_LOCATION(inode), &ino, sizeof(lb_addr));
		__udf_read_inode(inode);
		if (is_bad_inode(inode))
		{
			iput(inode);
			return NULL;
		}
	}

	if ( ino.logicalBlockNum >= UDF_SB_PARTLEN(sb, ino.partitionReferenceNum) )
	{
		udf_debug("block=%d, partition=%d out of range\n",
			ino.logicalBlockNum, ino.partitionReferenceNum);
		make_bad_inode(inode);
		iput(inode);
		return NULL;
 	}

	return inode;
}

int8_t udf_add_aext(struct inode *inode, lb_addr *bloc, int *extoffset,
	lb_addr eloc, uint32_t elen, struct buffer_head **bh, int inc)
{
	int adsize;
	short_ad *sad = NULL;
	long_ad *lad = NULL;
	struct allocExtDesc *aed;
	int8_t etype;

	if (!(*bh))
	{
		if (!(*bh = udf_tread(inode->i_sb,
			udf_get_lb_pblock(inode->i_sb, *bloc, 0))))
		{
			udf_debug("reading block %d failed!\n",
				udf_get_lb_pblock(inode->i_sb, *bloc, 0));
			return -1;
		}
	}

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_SHORT)
		adsize = sizeof(short_ad);
	else if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_LONG)
		adsize = sizeof(long_ad);
	else
		return -1;

	if (*extoffset + (2 * adsize) > inode->i_sb->s_blocksize)
	{
		char *sptr, *dptr;
		struct buffer_head *nbh;
		int err, loffset;
		lb_addr obloc = *bloc;

		if (!(bloc->logicalBlockNum = udf_new_block(inode->i_sb, inode,
			obloc.partitionReferenceNum, obloc.logicalBlockNum, &err)))
		{
			return -1;
		}
		if (!(nbh = udf_tgetblk(inode->i_sb, udf_get_lb_pblock(inode->i_sb,
			*bloc, 0))))
		{
			return -1;
		}
		lock_buffer(nbh);
		memset(nbh->b_data, 0x00, inode->i_sb->s_blocksize);
		mark_buffer_uptodate(nbh, 1);
		unlock_buffer(nbh);
		mark_buffer_dirty_inode(nbh, inode);

		aed = (struct allocExtDesc *)(nbh->b_data);
		if (!UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_STRICT))
			aed->previousAllocExtLocation = cpu_to_le32(obloc.logicalBlockNum);
		if (*extoffset + adsize > inode->i_sb->s_blocksize)
		{
			loffset = *extoffset;
			aed->lengthAllocDescs = cpu_to_le32(adsize);
			sptr = (*bh)->b_data + *extoffset - adsize;
			dptr = nbh->b_data + sizeof(struct allocExtDesc);
			memcpy(dptr, sptr, adsize);
			*extoffset = sizeof(struct allocExtDesc) + adsize;
		}
		else
		{
			loffset = *extoffset + adsize;
			aed->lengthAllocDescs = cpu_to_le32(0);
			sptr = (*bh)->b_data + *extoffset;
			*extoffset = sizeof(struct allocExtDesc);

			if (memcmp(&UDF_I_LOCATION(inode), &obloc, sizeof(lb_addr)))
			{
				aed = (struct allocExtDesc *)(*bh)->b_data;
				aed->lengthAllocDescs =
					cpu_to_le32(le32_to_cpu(aed->lengthAllocDescs) + adsize);
			}
			else
			{
				UDF_I_LENALLOC(inode) += adsize;
				mark_inode_dirty(inode);
			}
		}
		if (UDF_SB_UDFREV(inode->i_sb) >= 0x0200)
			udf_new_tag(nbh->b_data, TAG_IDENT_AED, 3, 1,
				bloc->logicalBlockNum, sizeof(tag));
		else
			udf_new_tag(nbh->b_data, TAG_IDENT_AED, 2, 1,
				bloc->logicalBlockNum, sizeof(tag));
		switch (UDF_I_ALLOCTYPE(inode))
		{
			case ICBTAG_FLAG_AD_SHORT:
			{
				sad = (short_ad *)sptr;
				sad->extLength = cpu_to_le32(
					EXT_NEXT_EXTENT_ALLOCDECS |
					inode->i_sb->s_blocksize);
				sad->extPosition = cpu_to_le32(bloc->logicalBlockNum);
				break;
			}
			case ICBTAG_FLAG_AD_LONG:
			{
				lad = (long_ad *)sptr;
				lad->extLength = cpu_to_le32(
					EXT_NEXT_EXTENT_ALLOCDECS |
					inode->i_sb->s_blocksize);
				lad->extLocation = cpu_to_lelb(*bloc);
				memset(lad->impUse, 0x00, sizeof(lad->impUse));
				break;
			}
		}
		if (!UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_STRICT) || UDF_SB_UDFREV(inode->i_sb) >= 0x0201)
			udf_update_tag((*bh)->b_data, loffset);
		else
			udf_update_tag((*bh)->b_data, sizeof(struct allocExtDesc));
		mark_buffer_dirty_inode(*bh, inode);
		udf_release_data(*bh);
		*bh = nbh;
	}

	etype = udf_write_aext(inode, *bloc, extoffset, eloc, elen, *bh, inc);

	if (!memcmp(&UDF_I_LOCATION(inode), bloc, sizeof(lb_addr)))
	{
		UDF_I_LENALLOC(inode) += adsize;
		mark_inode_dirty(inode);
	}
	else
	{
		aed = (struct allocExtDesc *)(*bh)->b_data;
		aed->lengthAllocDescs =
			cpu_to_le32(le32_to_cpu(aed->lengthAllocDescs) + adsize);
		if (!UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_STRICT) || UDF_SB_UDFREV(inode->i_sb) >= 0x0201)
			udf_update_tag((*bh)->b_data, *extoffset + (inc ? 0 : adsize));
		else
			udf_update_tag((*bh)->b_data, sizeof(struct allocExtDesc));
		mark_buffer_dirty_inode(*bh, inode);
	}

	return etype;
}

int8_t udf_write_aext(struct inode *inode, lb_addr bloc, int *extoffset,
    lb_addr eloc, uint32_t elen, struct buffer_head *bh, int inc)
{
	int adsize;
	short_ad *sad = NULL;
	long_ad *lad = NULL;

	if (!(bh))
	{
		if (!(bh = udf_tread(inode->i_sb,
			udf_get_lb_pblock(inode->i_sb, bloc, 0))))
		{
			udf_debug("reading block %d failed!\n",
				udf_get_lb_pblock(inode->i_sb, bloc, 0));
			return -1;
		}
	}
	else
		atomic_inc(&bh->b_count);

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_SHORT)
		adsize = sizeof(short_ad);
	else if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_LONG)
		adsize = sizeof(long_ad);
	else
		return -1;

	switch (UDF_I_ALLOCTYPE(inode))
	{
		case ICBTAG_FLAG_AD_SHORT:
		{
			sad = (short_ad *)((bh)->b_data + *extoffset);
			sad->extLength = cpu_to_le32(elen);
			sad->extPosition = cpu_to_le32(eloc.logicalBlockNum);
			break;
		}
		case ICBTAG_FLAG_AD_LONG:
		{
			lad = (long_ad *)((bh)->b_data + *extoffset);
			lad->extLength = cpu_to_le32(elen);
			lad->extLocation = cpu_to_lelb(eloc);
			memset(lad->impUse, 0x00, sizeof(lad->impUse));
			break;
		}
	}

	if (memcmp(&UDF_I_LOCATION(inode), &bloc, sizeof(lb_addr)))
	{
		if (!UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_STRICT) || UDF_SB_UDFREV(inode->i_sb) >= 0x0201)
		{
			struct allocExtDesc *aed = (struct allocExtDesc *)(bh)->b_data;
			udf_update_tag((bh)->b_data,
				le32_to_cpu(aed->lengthAllocDescs) + sizeof(struct allocExtDesc));
		}
		mark_buffer_dirty_inode(bh, inode);
	}
	else
	{
		mark_inode_dirty(inode);
		mark_buffer_dirty(bh);
	}

	if (inc)
		*extoffset += adsize;
	udf_release_data(bh);
	return (elen >> 30);
}

int8_t udf_next_aext(struct inode *inode, lb_addr *bloc, int *extoffset,
	lb_addr *eloc, uint32_t *elen, struct buffer_head **bh, int inc)
{
	uint16_t tagIdent;
	int pos, alen;
	int8_t etype;

	if (!(*bh))
	{
		if (!(*bh = udf_tread(inode->i_sb,
			udf_get_lb_pblock(inode->i_sb, *bloc, 0))))
		{
			udf_debug("reading block %d failed!\n",
				udf_get_lb_pblock(inode->i_sb, *bloc, 0));
			return -1;
		}
	}

	tagIdent = le16_to_cpu(((tag *)(*bh)->b_data)->tagIdent);

	if (!memcmp(&UDF_I_LOCATION(inode), bloc, sizeof(lb_addr)))
	{
		if (tagIdent == TAG_IDENT_FE || tagIdent == TAG_IDENT_EFE ||
			UDF_I_NEW_INODE(inode))
		{
			pos = udf_file_entry_alloc_offset(inode);
			alen = UDF_I_LENALLOC(inode) + pos;
		}
		else if (tagIdent == TAG_IDENT_USE)
		{
			pos = sizeof(struct unallocSpaceEntry);
			alen = UDF_I_LENALLOC(inode) + pos;
		}
		else
			return -1;
	}
	else if (tagIdent == TAG_IDENT_AED)
	{
		struct allocExtDesc *aed = (struct allocExtDesc *)(*bh)->b_data;

		pos = sizeof(struct allocExtDesc);
		alen = le32_to_cpu(aed->lengthAllocDescs) + pos;
	}
	else
		return -1;

	if (!(*extoffset))
		*extoffset = pos;

	switch (UDF_I_ALLOCTYPE(inode))
	{
		case ICBTAG_FLAG_AD_SHORT:
		{
			short_ad *sad;

			if (!(sad = udf_get_fileshortad((*bh)->b_data, alen, extoffset, inc)))
				return -1;

			if ((etype = le32_to_cpu(sad->extLength) >> 30) == (EXT_NEXT_EXTENT_ALLOCDECS >> 30))
			{
				bloc->logicalBlockNum = le32_to_cpu(sad->extPosition);
				*extoffset = 0;
				udf_release_data(*bh);
				*bh = NULL;
				return udf_next_aext(inode, bloc, extoffset, eloc, elen, bh, inc);
			}
			else
			{
				eloc->logicalBlockNum = le32_to_cpu(sad->extPosition);
				eloc->partitionReferenceNum = UDF_I_LOCATION(inode).partitionReferenceNum;
				*elen = le32_to_cpu(sad->extLength) & UDF_EXTENT_LENGTH_MASK;
			}
			break;
		}
		case ICBTAG_FLAG_AD_LONG:
		{
			long_ad *lad;

			if (!(lad = udf_get_filelongad((*bh)->b_data, alen, extoffset, inc)))
				return -1;

			if ((etype = le32_to_cpu(lad->extLength) >> 30) == (EXT_NEXT_EXTENT_ALLOCDECS >> 30))
			{
				*bloc = lelb_to_cpu(lad->extLocation);
				*extoffset = 0;
				udf_release_data(*bh);
				*bh = NULL;
				return udf_next_aext(inode, bloc, extoffset, eloc, elen, bh, inc);
			}
			else
			{
				*eloc = lelb_to_cpu(lad->extLocation);
				*elen = le32_to_cpu(lad->extLength) & UDF_EXTENT_LENGTH_MASK;
			}
			break;
		}
		case ICBTAG_FLAG_AD_IN_ICB:
		{
			if (UDF_I_LENALLOC(inode) == 0)
				return -1;
			etype = (EXT_RECORDED_ALLOCATED >> 30);
			*eloc = UDF_I_LOCATION(inode);
			*elen = UDF_I_LENALLOC(inode);
			break;
		}
		default:
		{
			udf_debug("alloc_type = %d unsupported\n", UDF_I_ALLOCTYPE(inode));
			return -1;
		}
	}
	if (*elen)
		return etype;

	udf_debug("Empty Extent, inode=%ld, alloctype=%d, eloc=%d, elen=%d, etype=%d, extoffset=%d\n",
		inode->i_ino, UDF_I_ALLOCTYPE(inode), eloc->logicalBlockNum, *elen, etype, *extoffset);
	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_SHORT)
		*extoffset -= sizeof(short_ad);
	else if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_LONG)
		*extoffset -= sizeof(long_ad);
	return -1;
}

int8_t udf_current_aext(struct inode *inode, lb_addr *bloc, int *extoffset,
	lb_addr *eloc, uint32_t *elen, struct buffer_head **bh, int inc)
{
	int pos, alen;
	int8_t etype;

	if (!(*bh))
	{
		if (!(*bh = udf_tread(inode->i_sb,
			udf_get_lb_pblock(inode->i_sb, *bloc, 0))))
		{
			udf_debug("reading block %d failed!\n",
				udf_get_lb_pblock(inode->i_sb, *bloc, 0));
			return -1;
		}
	}

	if (!memcmp(&UDF_I_LOCATION(inode), bloc, sizeof(lb_addr)))
	{
		if (!(UDF_I_EXTENDED_FE(inode)))
			pos = sizeof(struct fileEntry) + UDF_I_LENEATTR(inode);
		else
			pos = sizeof(struct extendedFileEntry) + UDF_I_LENEATTR(inode);
		alen = UDF_I_LENALLOC(inode) + pos;
	}
	else
	{
		struct allocExtDesc *aed = (struct allocExtDesc *)(*bh)->b_data;

		pos = sizeof(struct allocExtDesc);
		alen = le32_to_cpu(aed->lengthAllocDescs) + pos;
	}

	if (!(*extoffset))
		*extoffset = pos;

	switch (UDF_I_ALLOCTYPE(inode))
	{
		case ICBTAG_FLAG_AD_SHORT:
		{
			short_ad *sad;

			if (!(sad = udf_get_fileshortad((*bh)->b_data, alen, extoffset, inc)))
				return -1;

			etype = le32_to_cpu(sad->extLength) >> 30;
			eloc->logicalBlockNum = le32_to_cpu(sad->extPosition);
			eloc->partitionReferenceNum = UDF_I_LOCATION(inode).partitionReferenceNum;
			*elen = le32_to_cpu(sad->extLength) & UDF_EXTENT_LENGTH_MASK;
			break;
		}
		case ICBTAG_FLAG_AD_LONG:
		{
			long_ad *lad;

			if (!(lad = udf_get_filelongad((*bh)->b_data, alen, extoffset, inc)))
				return -1;

			etype = le32_to_cpu(lad->extLength) >> 30;
			*eloc = lelb_to_cpu(lad->extLocation);
			*elen = le32_to_cpu(lad->extLength) & UDF_EXTENT_LENGTH_MASK;
			break;
		}
		default:
		{
			udf_debug("alloc_type = %d unsupported\n", UDF_I_ALLOCTYPE(inode));
			return -1;
		}
	}
	if (*elen)
		return etype;

	udf_debug("Empty Extent!\n");
	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_SHORT)
		*extoffset -= sizeof(short_ad);
	else if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_LONG)
		*extoffset -= sizeof(long_ad);
	return -1;
}

int8_t udf_insert_aext(struct inode *inode, lb_addr bloc, int extoffset,
	lb_addr neloc, uint32_t nelen, struct buffer_head *bh)
{
	lb_addr oeloc;
	uint32_t oelen;
	int8_t etype;

	if (!bh)
	{
		if (!(bh = udf_tread(inode->i_sb,
			udf_get_lb_pblock(inode->i_sb, bloc, 0))))
		{
			udf_debug("reading block %d failed!\n",
				udf_get_lb_pblock(inode->i_sb, bloc, 0));
			return -1;
		}
	}
	else
		atomic_inc(&bh->b_count);

	while ((etype = udf_next_aext(inode, &bloc, &extoffset, &oeloc, &oelen, &bh, 0)) != -1)
	{
		udf_write_aext(inode, bloc, &extoffset, neloc, nelen, bh, 1);

		neloc = oeloc;
		nelen = (etype << 30) | oelen;
	}
	udf_add_aext(inode, &bloc, &extoffset, neloc, nelen, &bh, 1);
	udf_release_data(bh);
	return (nelen >> 30);
}

int8_t udf_delete_aext(struct inode *inode, lb_addr nbloc, int nextoffset,
	lb_addr eloc, uint32_t elen, struct buffer_head *nbh)
{
	struct buffer_head *obh;
	lb_addr obloc;
	int oextoffset, adsize;
	int8_t etype;
	struct allocExtDesc *aed;

	if (!(nbh))
	{
		if (!(nbh = udf_tread(inode->i_sb,
			udf_get_lb_pblock(inode->i_sb, nbloc, 0))))
		{
			udf_debug("reading block %d failed!\n",
				udf_get_lb_pblock(inode->i_sb, nbloc, 0));
			return -1;
		}
	}
	else
		atomic_inc(&nbh->b_count);
	atomic_inc(&nbh->b_count);

	if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_SHORT)
		adsize = sizeof(short_ad);
	else if (UDF_I_ALLOCTYPE(inode) == ICBTAG_FLAG_AD_LONG)
		adsize = sizeof(long_ad);
	else
		adsize = 0;

	obh = nbh;
	obloc = nbloc;
	oextoffset = nextoffset;

	if (udf_next_aext(inode, &nbloc, &nextoffset, &eloc, &elen, &nbh, 1) == -1)
		return -1;

	while ((etype = udf_next_aext(inode, &nbloc, &nextoffset, &eloc, &elen, &nbh, 1)) != -1)
	{
		udf_write_aext(inode, obloc, &oextoffset, eloc, (etype << 30) | elen, obh, 1);
		if (memcmp(&nbloc, &obloc, sizeof(lb_addr)))
		{
			obloc = nbloc;
			udf_release_data(obh);
			atomic_inc(&nbh->b_count);
			obh = nbh;
			oextoffset = nextoffset - adsize;
		}
	}
	memset(&eloc, 0x00, sizeof(lb_addr));
	elen = 0;

	if (memcmp(&nbloc, &obloc, sizeof(lb_addr)))
	{
		udf_free_blocks(inode->i_sb, inode, nbloc, 0, 1);
		udf_write_aext(inode, obloc, &oextoffset, eloc, elen, obh, 1);
		udf_write_aext(inode, obloc, &oextoffset, eloc, elen, obh, 1);
		if (!memcmp(&UDF_I_LOCATION(inode), &obloc, sizeof(lb_addr)))
		{
			UDF_I_LENALLOC(inode) -= (adsize * 2);
			mark_inode_dirty(inode);
		}
		else
		{
			aed = (struct allocExtDesc *)(obh)->b_data;
			aed->lengthAllocDescs =
				cpu_to_le32(le32_to_cpu(aed->lengthAllocDescs) - (2*adsize));
			if (!UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_STRICT) || UDF_SB_UDFREV(inode->i_sb) >= 0x0201)
				udf_update_tag((obh)->b_data, oextoffset - (2*adsize));
			else
				udf_update_tag((obh)->b_data, sizeof(struct allocExtDesc));
			mark_buffer_dirty_inode(obh, inode);
		}
	}
	else
	{
		udf_write_aext(inode, obloc, &oextoffset, eloc, elen, obh, 1);
		if (!memcmp(&UDF_I_LOCATION(inode), &obloc, sizeof(lb_addr)))
		{
			UDF_I_LENALLOC(inode) -= adsize;
			mark_inode_dirty(inode);
		}
		else
		{
			aed = (struct allocExtDesc *)(obh)->b_data;
			aed->lengthAllocDescs =
				cpu_to_le32(le32_to_cpu(aed->lengthAllocDescs) - adsize);
			if (!UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_STRICT) || UDF_SB_UDFREV(inode->i_sb) >= 0x0201)
				udf_update_tag((obh)->b_data, oextoffset - adsize);
			else
				udf_update_tag((obh)->b_data, sizeof(struct allocExtDesc));
			mark_buffer_dirty_inode(obh, inode);
		}
	}
	
	udf_release_data(nbh);
	udf_release_data(obh);
	return (elen >> 30);
}

int8_t inode_bmap(struct inode *inode, int block, lb_addr *bloc, uint32_t *extoffset,
	lb_addr *eloc, uint32_t *elen, uint32_t *offset, struct buffer_head **bh)
{
	uint64_t lbcount = 0, bcount = (uint64_t)block << inode->i_sb->s_blocksize_bits;
	int8_t etype;

	if (block < 0)
	{
		printk(KERN_ERR "udf: inode_bmap: block < 0\n");
		return -1;
	}
	if (!inode)
	{
		printk(KERN_ERR "udf: inode_bmap: NULL inode\n");
		return -1;
	}

	*extoffset = 0;
	*elen = 0;
	*bloc = UDF_I_LOCATION(inode);

	do
	{
		if ((etype = udf_next_aext(inode, bloc, extoffset, eloc, elen, bh, 1)) == -1)
		{
			*offset = bcount - lbcount;
			UDF_I_LENEXTENTS(inode) = lbcount;
			return -1;
		}
		lbcount += *elen;
	} while (lbcount <= bcount);

	*offset = bcount + *elen - lbcount;

	return etype;
}

long udf_block_map(struct inode *inode, long block)
{
	lb_addr eloc, bloc;
	uint32_t offset, extoffset, elen;
	struct buffer_head *bh = NULL;
	int ret;

	lock_kernel();

	if (inode_bmap(inode, block, &bloc, &extoffset, &eloc, &elen, &offset, &bh) == (EXT_RECORDED_ALLOCATED >> 30))
		ret = udf_get_lb_pblock(inode->i_sb, eloc, offset >> inode->i_sb->s_blocksize_bits);
	else
		ret = 0;

	unlock_kernel();

	if (bh)
		udf_release_data(bh);

	if (UDF_QUERY_FLAG(inode->i_sb, UDF_FLAG_VARCONV))
		return udf_fixed_to_variable(ret);
	else
		return ret;
}
