/*
 * Copyright 1999-2002 Hans Reiser, see reiserfs/README for licensing and copyright details
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/reiserfs_fs.h>
#include <linux/locks.h>

/* access to tail : when one is going to read tail it must make sure, that is not running.
 direct2indirect and indirect2direct can not run concurrently */


/* Converts direct items to an unformatted node. Panics if file has no
   tail. -ENOSPC if no disk space for conversion */
/* path points to first direct item of the file regarless of how many of
   them are there */
int direct2indirect (struct reiserfs_transaction_handle *th, struct inode * inode, 
		     struct path * path, struct buffer_head * unbh,
		     loff_t tail_offset)
{
    struct super_block * sb = inode->i_sb;
    struct buffer_head *up_to_date_bh ;
    struct item_head * p_le_ih = PATH_PITEM_HEAD (path);
    unsigned long total_tail = 0 ;
    struct cpu_key end_key;  /* Key to search for the last byte of the
				converted item. */
    struct item_head ind_ih; /* new indirect item to be inserted or
                                key of unfm pointer to be pasted */
    int	n_blk_size,
      n_retval;	  /* returned value for reiserfs_insert_item and clones */
    unp_t unfm_ptr;  /* Handle on an unformatted node
				       that will be inserted in the
				       tree. */


    sb->u.reiserfs_sb.s_direct2indirect ++;

    n_blk_size = sb->s_blocksize;

    /* and key to search for append or insert pointer to the new
       unformatted node. */
    copy_item_head (&ind_ih, p_le_ih);
    set_le_ih_k_offset (&ind_ih, tail_offset);
    set_le_ih_k_type (&ind_ih, TYPE_INDIRECT);

    /* Set the key to search for the place for new unfm pointer */
    make_cpu_key (&end_key, inode, tail_offset, TYPE_INDIRECT, 4);

    // FIXME: we could avoid this 
    if ( search_for_position_by_key (sb, &end_key, path) == POSITION_FOUND ) {
	reiserfs_warning (sb, "PAP-14030: direct2indirect: "
			"pasted or inserted byte exists in the tree %K. "
			"Use fsck to repair.\n", &end_key);
	pathrelse(path);
	return -EIO;
    }
    
    p_le_ih = PATH_PITEM_HEAD (path);

    unfm_ptr = cpu_to_le32 (unbh->b_blocknr);
    
    if ( is_statdata_le_ih (p_le_ih) )  {
	/* Insert new indirect item. */
	set_ih_free_space (&ind_ih, 0); /* delete at nearest future */
        put_ih_item_len( &ind_ih, UNFM_P_SIZE );
	PATH_LAST_POSITION (path)++;
	n_retval = reiserfs_insert_item (th, path, &end_key, &ind_ih, 
					 (char *)&unfm_ptr);
    } else {
	/* Paste into last indirect item of an object. */
	n_retval = reiserfs_paste_into_item(th, path, &end_key,
					    (char *)&unfm_ptr, UNFM_P_SIZE);
    }
    if ( n_retval ) {
	return n_retval;
    }

    // note: from here there are two keys which have matching first
    // three key components. They only differ by the fourth one.


    /* Set the key to search for the direct items of the file */
    make_cpu_key (&end_key, inode, max_reiserfs_offset (inode), TYPE_DIRECT, 4);

    /* Move bytes from the direct items to the new unformatted node
       and delete them. */
    while (1)  {
	int tail_size;

	/* end_key.k_offset is set so, that we will always have found
           last item of the file */
	if ( search_for_position_by_key (sb, &end_key, path) == POSITION_FOUND )
	    reiserfs_panic (sb, "PAP-14050: direct2indirect: "
			    "direct item (%K) not found", &end_key);
	p_le_ih = PATH_PITEM_HEAD (path);
	RFALSE( !is_direct_le_ih (p_le_ih),
	        "vs-14055: direct item expected(%K), found %h",
                &end_key, p_le_ih);
        tail_size = (le_ih_k_offset (p_le_ih) & (n_blk_size - 1))
            + ih_item_len(p_le_ih) - 1;

	/* we only send the unbh pointer if the buffer is not up to date.
	** this avoids overwriting good data from writepage() with old data
	** from the disk or buffer cache
	** Special case: unbh->b_page will be NULL if we are coming through
	** DIRECT_IO handler here.
	*/
	if ( !unbh->b_page || buffer_uptodate(unbh) || Page_Uptodate(unbh->b_page)) {
	    up_to_date_bh = NULL ;
	} else {
	    up_to_date_bh = unbh ;
	}
	n_retval = reiserfs_delete_item (th, path, &end_key, inode, 
	                                 up_to_date_bh) ;

	total_tail += n_retval ;
	if (tail_size == n_retval)
	    // done: file does not have direct items anymore
	    break;

    }
    /* if we've copied bytes from disk into the page, we need to zero
    ** out the unused part of the block (it was not up to date before)
    ** the page is still kmapped (by whoever called reiserfs_get_block)
    */
    if (up_to_date_bh) {
        unsigned pgoff = (tail_offset + total_tail - 1) & (PAGE_CACHE_SIZE - 1);
	memset(page_address(unbh->b_page) + pgoff, 0, n_blk_size - total_tail) ;
    }

    inode->u.reiserfs_i.i_first_direct_byte = U32_MAX;

    reiserfs_update_tail_transaction(inode);
    return 0;
}


/* stolen from fs/buffer.c */
void reiserfs_unmap_buffer(struct buffer_head *bh) {
  if (buffer_mapped(bh)) {
    if (buffer_journaled(bh) || buffer_journal_dirty(bh)) {
      BUG() ;
    }
    mark_buffer_clean(bh) ;
    lock_buffer(bh) ;
    clear_bit(BH_Mapped, &bh->b_state) ;
    clear_bit(BH_Req, &bh->b_state) ;
    clear_bit(BH_New, &bh->b_state) ;
    unlock_buffer(bh) ;
  }
}

static void
unmap_buffers(struct page *page, loff_t pos) {
  struct buffer_head *bh ;
  struct buffer_head *head ;
  struct buffer_head *next ;
  unsigned long tail_index ;
  unsigned long cur_index ;

  if (page) {
    if (page->buffers) {
      tail_index = pos & (PAGE_CACHE_SIZE - 1) ;
      cur_index = 0 ;
      head = page->buffers ;
      bh = head ;
      do {
	next = bh->b_this_page ;

        /* we want to unmap the buffers that contain the tail, and
        ** all the buffers after it (since the tail must be at the
        ** end of the file).  We don't want to unmap file data 
        ** before the tail, since it might be dirty and waiting to 
        ** reach disk
        */
        cur_index += bh->b_size ;
        if (cur_index > tail_index) {
          reiserfs_unmap_buffer(bh) ;
        }
	bh = next ;
      } while (bh != head) ;
    }
  } 
}

/* this first locks inode (neither reads nor sync are permitted),
   reads tail through page cache, insert direct item. When direct item
   inserted successfully inode is left locked. Return value is always
   what we expect from it (number of cut bytes). But when tail remains
   in the unformatted node, we set mode to SKIP_BALANCING and unlock
   inode */
int indirect2direct (struct reiserfs_transaction_handle *th, 
		     struct inode * p_s_inode,
		     struct page *page, 
		     struct path * p_s_path, /* path to the indirect item. */
		     const struct cpu_key * p_s_item_key, /* Key to look for unformatted node pointer to be cut. */
		     loff_t n_new_file_size, /* New file size. */
		     char * p_c_mode)
{
    struct super_block * p_s_sb = p_s_inode->i_sb;
    struct item_head      s_ih;
    unsigned long n_block_size = p_s_sb->s_blocksize;
    char * tail;
    int tail_len, round_tail_len;
    loff_t pos, pos1; /* position of first byte of the tail */
    struct cpu_key key;

    p_s_sb->u.reiserfs_sb.s_indirect2direct ++;

    *p_c_mode = M_SKIP_BALANCING;

    /* store item head path points to. */
    copy_item_head (&s_ih, PATH_PITEM_HEAD(p_s_path));

    tail_len = (n_new_file_size & (n_block_size - 1));
    if (get_inode_sd_version (p_s_inode) == STAT_DATA_V2)
	round_tail_len = ROUND_UP (tail_len);
    else
	round_tail_len = tail_len;

    pos = le_ih_k_offset (&s_ih) - 1 + (ih_item_len(&s_ih) / UNFM_P_SIZE - 1) * p_s_sb->s_blocksize;
    pos1 = pos;

    // we are protected by i_sem. The tail can not disapper, not
    // append can be done either
    // we are in truncate or packing tail in file_release

    tail = (char *)kmap(page) ; /* this can schedule */

    if (path_changed (&s_ih, p_s_path)) {
	/* re-search indirect item */
	if ( search_for_position_by_key (p_s_sb, p_s_item_key, p_s_path) == POSITION_NOT_FOUND )
	    reiserfs_panic(p_s_sb, "PAP-5520: indirect2direct: "
			   "item to be converted %K does not exist", p_s_item_key);
	copy_item_head(&s_ih, PATH_PITEM_HEAD(p_s_path));
#ifdef CONFIG_REISERFS_CHECK
	pos = le_ih_k_offset (&s_ih) - 1 + 
	    (ih_item_len(&s_ih) / UNFM_P_SIZE - 1) * p_s_sb->s_blocksize;
	if (pos != pos1)
	    reiserfs_panic (p_s_sb, "vs-5530: indirect2direct: "
			    "tail position changed while we were reading it");
#endif
    }


    /* Set direct item header to insert. */
    make_le_item_head (&s_ih, 0, get_inode_item_key_version (p_s_inode), pos1 + 1,
		       TYPE_DIRECT, round_tail_len, 0xffff/*ih_free_space*/);

    /* we want a pointer to the first byte of the tail in the page.
    ** the page was locked and this part of the page was up to date when
    ** indirect2direct was called, so we know the bytes are still valid
    */
    tail = tail + (pos & (PAGE_CACHE_SIZE - 1)) ;

    PATH_LAST_POSITION(p_s_path)++;

    key = *p_s_item_key;
    set_cpu_key_k_type (&key, TYPE_DIRECT);
    key.key_length = 4;
    /* Insert tail as new direct item in the tree */
    if ( reiserfs_insert_item(th, p_s_path, &key, &s_ih,
			      tail ? tail : NULL) < 0 ) {
	/* No disk memory. So we can not convert last unformatted node
	   to the direct item.  In this case we used to adjust
	   indirect items's ih_free_space. Now ih_free_space is not
	   used, it would be ideal to write zeros to corresponding
	   unformatted node. For now i_size is considered as guard for
	   going out of file size */
	kunmap(page) ;
	return n_block_size - round_tail_len;
    }
    kunmap(page) ;

    /* this will invalidate all the buffers in the page after
    ** pos1
    */
    unmap_buffers(page, pos1) ;

    // note: we have now the same as in above direct2indirect
    // conversion: there are two keys which have matching first three
    // key components. They only differ by the fouhth one.

    /* We have inserted new direct item and must remove last
       unformatted node. */
    p_s_inode->i_blocks += (p_s_sb->s_blocksize / 512);
    *p_c_mode = M_CUT;

    /* we store position of first direct item in the in-core inode */
    //mark_file_with_tail (p_s_inode, pos1 + 1);
    p_s_inode->u.reiserfs_i.i_first_direct_byte = pos1 + 1;

    return n_block_size - round_tail_len;
}



