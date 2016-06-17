/* 
 * Copyright 2000-2002 by Hans Reiser, licensing governed by reiserfs/README
 */
 
/* 
 * Written by Alexander Zarochentcev.
 *
 * The kernel part of the (on-line) reiserfs resizer.
 */

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/locks.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/reiserfs_fs.h>
#include <linux/reiserfs_fs_sb.h>

int reiserfs_resize (struct super_block * s, unsigned long block_count_new)
{
	struct reiserfs_super_block * sb;
        struct reiserfs_bitmap_info *bitmap;
	struct buffer_head * bh;
	struct reiserfs_transaction_handle th;
	unsigned int bmap_nr_new, bmap_nr;
	unsigned int block_r_new, block_r;
	
	struct reiserfs_list_bitmap * jb;
	struct reiserfs_list_bitmap jbitmap[JOURNAL_NUM_BITMAPS];
	
	unsigned long int block_count, free_blocks;
	int i;
	int copy_size ;

	sb = SB_DISK_SUPER_BLOCK(s);

	if (SB_BLOCK_COUNT(s) >= block_count_new) {
		reiserfs_warning(s, "can\'t shrink filesystem on-line\n");
		return -EINVAL;
	}

	/* check the device size */
	bh = sb_bread(s, block_count_new - 1);
	if (!bh) {
		reiserfs_warning(s, "reiserfs_resize: can\'t read last block\n");
		return -EINVAL;
	}	
	bforget(bh);

	/* old disk layout detection; those partitions can be mounted, but
	 * cannot be resized */
	if (SB_BUFFER_WITH_SB(s)->b_blocknr *	SB_BUFFER_WITH_SB(s)->b_size 
		!= REISERFS_DISK_OFFSET_IN_BYTES ) {
		reiserfs_warning(s, "reiserfs_resize: unable to resize a reiserfs without distributed bitmap (fs version < 3.5.12)\n");
		return -ENOTSUPP;
	}
       
	/* count used bits in last bitmap block */
	block_r = SB_BLOCK_COUNT(s) -
	        (SB_BMAP_NR(s) - 1) * s->s_blocksize * 8;
	
	/* count bitmap blocks in new fs */
	bmap_nr_new = block_count_new / ( s->s_blocksize * 8 );
	block_r_new = block_count_new - bmap_nr_new * s->s_blocksize * 8;
	if (block_r_new) 
		bmap_nr_new++;
	else
		block_r_new = s->s_blocksize * 8;

	/* save old values */
	block_count = SB_BLOCK_COUNT(s);
	bmap_nr     = SB_BMAP_NR(s);

	/* resizing of reiserfs bitmaps (journal and real), if needed */
	if (bmap_nr_new > bmap_nr) {	    
	    /* reallocate journal bitmaps */
	    if (reiserfs_allocate_list_bitmaps(s, jbitmap, bmap_nr_new) < 0) {
		reiserfs_warning(s, "reiserfs_resize: unable to allocate memory for journal bitmaps\n");
		unlock_super(s) ;
		return -ENOMEM ;
	    }
	    /* the new journal bitmaps are zero filled, now we copy in the bitmap
	    ** node pointers from the old journal bitmap structs, and then
	    ** transfer the new data structures into the journal struct.
	    **
	    ** using the copy_size var below allows this code to work for
	    ** both shrinking and expanding the FS.
	    */
	    copy_size = bmap_nr_new < bmap_nr ? bmap_nr_new : bmap_nr ;
	    copy_size = copy_size * sizeof(struct reiserfs_list_bitmap_node *) ;
	    for (i = 0 ; i < JOURNAL_NUM_BITMAPS ; i++) {
		struct reiserfs_bitmap_node **node_tmp ;
		jb = SB_JOURNAL(s)->j_list_bitmap + i ;
		memcpy(jbitmap[i].bitmaps, jb->bitmaps, copy_size) ;

		/* just in case vfree schedules on us, copy the new
		** pointer into the journal struct before freeing the 
		** old one
		*/
		node_tmp = jb->bitmaps ;
		jb->bitmaps = jbitmap[i].bitmaps ;
		vfree(node_tmp) ;
	    }	
	
	    /* allocate additional bitmap blocks, reallocate array of bitmap
	     * block pointers */
	    bitmap = vmalloc(sizeof(struct reiserfs_bitmap_info) * bmap_nr_new);
	    if (!bitmap) {
		reiserfs_warning(s, "reiserfs_resize: unable to allocate memory.\n");
		return -ENOMEM;
	    }
	    for (i = 0; i < bmap_nr; i++)
		bitmap[i] = SB_AP_BITMAP(s)[i];
	    for (i = bmap_nr; i < bmap_nr_new; i++) {
		bitmap[i].bh = sb_getblk(s, i * s->s_blocksize * 8);
		memset(bitmap[i].bh->b_data, 0, sb_blocksize(sb));
		reiserfs_test_and_set_le_bit(0, bitmap[i].bh->b_data);

		mark_buffer_dirty(bitmap[i].bh) ;
		mark_buffer_uptodate(bitmap[i].bh, 1);
		ll_rw_block(WRITE, 1, &bitmap[i].bh);
		wait_on_buffer(bitmap[i].bh);
		bitmap[i].first_zero_hint=1;
		bitmap[i].free_count = s->s_blocksize * 8 - 1;
	    }	
	    /* free old bitmap blocks array */
	    vfree(SB_AP_BITMAP(s)); 
			   
	    SB_AP_BITMAP(s) = bitmap;
	}
	
	/* begin transaction */
	journal_begin(&th, s, 10);

	/* correct last bitmap blocks in old and new disk layout */
	reiserfs_prepare_for_journal(s, SB_AP_BITMAP(s)[bmap_nr - 1].bh, 1);
	for (i = block_r; i < s->s_blocksize * 8; i++)
	    reiserfs_test_and_clear_le_bit(i, 
					   SB_AP_BITMAP(s)[bmap_nr - 1].bh->b_data);
	SB_AP_BITMAP(s)[bmap_nr - 1].free_count += s->s_blocksize * 8 - block_r;
	if ( !SB_AP_BITMAP(s)[bmap_nr - 1].first_zero_hint)
	    SB_AP_BITMAP(s)[bmap_nr - 1].first_zero_hint = block_r;
	journal_mark_dirty(&th, s, SB_AP_BITMAP(s)[bmap_nr - 1].bh);

	reiserfs_prepare_for_journal(s, SB_AP_BITMAP(s)[bmap_nr_new - 1].bh, 1);
	for (i = block_r_new; i < s->s_blocksize * 8; i++)
	    reiserfs_test_and_set_le_bit(i,
					 SB_AP_BITMAP(s)[bmap_nr_new - 1].bh->b_data);
	journal_mark_dirty(&th, s, SB_AP_BITMAP(s)[bmap_nr_new - 1].bh);

	SB_AP_BITMAP(s)[bmap_nr_new - 1].free_count -= s->s_blocksize * 8 - block_r_new;
	/* Extreme case where last bitmap is the only valid block in itself. */
	if ( !SB_AP_BITMAP(s)[bmap_nr_new - 1].free_count )
	    SB_AP_BITMAP(s)[bmap_nr_new - 1].first_zero_hint = 0;
 	/* update super */
	reiserfs_prepare_for_journal(s, SB_BUFFER_WITH_SB(s), 1) ;
	free_blocks = SB_FREE_BLOCKS(s);
	PUT_SB_FREE_BLOCKS(s, free_blocks + (block_count_new - block_count - (bmap_nr_new - bmap_nr)));
	PUT_SB_BLOCK_COUNT(s, block_count_new);
	PUT_SB_BMAP_NR(s, bmap_nr_new);
	s->s_dirt = 1;

	journal_mark_dirty(&th, s, SB_BUFFER_WITH_SB(s));
	
	SB_JOURNAL(s)->j_must_wait = 1;
	journal_end(&th, s, 10);

	return 0;
}

