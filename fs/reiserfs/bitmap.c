/*
 * Copyright 2000-2002 by Hans Reiser, licensing governed by reiserfs/README
 */
   
/* Reiserfs block (de)allocator, bitmap-based. */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <linux/kernel.h>

#include <linux/reiserfs_fs.h>
#include <linux/reiserfs_fs_sb.h>
#include <linux/reiserfs_fs_i.h>

#define PREALLOCATION_SIZE 9

#define INODE_INFO(inode) (&(inode)->u.reiserfs_i)

/* different reiserfs block allocator options */

#define SB_ALLOC_OPTS(s) ((s)->u.reiserfs_sb.s_alloc_options.bits)

#define  _ALLOC_concentrating_formatted_nodes 0
#define  _ALLOC_displacing_large_files 1
#define  _ALLOC_displacing_new_packing_localities 2
#define  _ALLOC_old_hashed_relocation 3
#define  _ALLOC_new_hashed_relocation 4
#define  _ALLOC_skip_busy 5
#define  _ALLOC_displace_based_on_dirid 6
#define  _ALLOC_hashed_formatted_nodes 7
#define  _ALLOC_old_way 8
#define  _ALLOC_hundredth_slices 9

#define  concentrating_formatted_nodes(s)     test_bit(_ALLOC_concentrating_formatted_nodes, &SB_ALLOC_OPTS(s))
#define  displacing_large_files(s)            test_bit(_ALLOC_displacing_large_files, &SB_ALLOC_OPTS(s))
#define  displacing_new_packing_localities(s) test_bit(_ALLOC_displacing_new_packing_localities, &SB_ALLOC_OPTS(s))

#define SET_OPTION(optname) \
   do { \
        reiserfs_warning(s, "reiserfs: option \"%s\" is set\n", #optname); \
        set_bit(_ALLOC_ ## optname , &SB_ALLOC_OPTS(s)); \
    } while(0)
#define TEST_OPTION(optname, s) \
    test_bit(_ALLOC_ ## optname , &SB_ALLOC_OPTS(s))


/* #define LIMIT(a,b) do { if ((a) > (b)) (a) = (b); } while(0) */

static inline void get_bit_address (struct super_block * s,
				    unsigned long block, int * bmap_nr, int * offset)
{
    /* It is in the bitmap block number equal to the block
     * number divided by the number of bits in a block. */
    *bmap_nr = block / (s->s_blocksize << 3);
    /* Within that bitmap block it is located at bit offset *offset. */
    *offset = block & ((s->s_blocksize << 3) - 1 );
    return;
}

#ifdef CONFIG_REISERFS_CHECK
int is_reusable (struct super_block * s, unsigned long block, int bit_value)
{
    int i, j;

    if (block == 0 || block >= SB_BLOCK_COUNT (s)) {
	reiserfs_warning (s, "vs-4010: is_reusable: block number is out of range %lu (%u)\n",
			  block, SB_BLOCK_COUNT (s));
	return 0;
    }

    /* it can't be one of the bitmap blocks */
    for (i = 0; i < SB_BMAP_NR (s); i ++)
	if (block == SB_AP_BITMAP (s)[i].bh->b_blocknr) {
	    reiserfs_warning (s, "vs: 4020: is_reusable: "
			      "bitmap block %lu(%u) can't be freed or reused\n",
			      block, SB_BMAP_NR (s));
	    return 0;
	}
  
    get_bit_address (s, block, &i, &j);

    if (i >= SB_BMAP_NR (s)) {
	reiserfs_warning (s, "vs-4030: is_reusable: there is no so many bitmap blocks: "
			  "block=%lu, bitmap_nr=%d\n", block, i);
	return 0;
    }

    if ((bit_value == 0 && 
         reiserfs_test_le_bit(j, SB_AP_BITMAP(s)[i].bh->b_data)) ||
	(bit_value == 1 && 
	 reiserfs_test_le_bit(j, SB_AP_BITMAP (s)[i].bh->b_data) == 0)) {
	reiserfs_warning (s, "vs-4040: is_reusable: corresponding bit of block %lu does not "
			  "match required value (i==%d, j==%d) test_bit==%d\n",
		block, i, j, reiserfs_test_le_bit (j, SB_AP_BITMAP (s)[i].bh->b_data));
		
	return 0;
    }

    if (bit_value == 0 && block == SB_ROOT_BLOCK (s)) {
	reiserfs_warning (s, "vs-4050: is_reusable: this is root block (%u), "
			  "it must be busy\n", SB_ROOT_BLOCK (s));
	return 0;
    }

    return 1;
}
#endif /* CONFIG_REISERFS_CHECK */

/* searches in journal structures for a given block number (bmap, off). If block
   is found in reiserfs journal it suggests next free block candidate to test. */
static inline  int is_block_in_journal (struct super_block * s, int bmap, int off, int *next)
{
    unsigned int tmp;

    if (reiserfs_in_journal (s, s->s_dev, bmap, off, s->s_blocksize, 1, &tmp)) {
	if (tmp) {		/* hint supplied */
	    *next = tmp;
	    PROC_INFO_INC( s, scan_bitmap.in_journal_hint );
	} else {
	    (*next) = off + 1;		/* inc offset to avoid looping. */
	    PROC_INFO_INC( s, scan_bitmap.in_journal_nohint );
	}
	PROC_INFO_INC( s, scan_bitmap.retry );
	return 1;
    }
    return 0;
}

/* it searches for a window of zero bits with given minimum and maximum lengths in one bitmap
 * block; */
static int scan_bitmap_block (struct reiserfs_transaction_handle *th,
			      int bmap_n, int *beg, int boundary, int min, int max, int unfm)
{
    struct super_block *s = th->t_super;
    struct reiserfs_bitmap_info *bi=&SB_AP_BITMAP(s)[bmap_n];
    int end, next;
    int org = *beg;

    RFALSE(bmap_n >= SB_BMAP_NR (s), "Bitmap %d is out of range (0..%d)\n",bmap_n, SB_BMAP_NR (s) - 1);
    PROC_INFO_INC( s, scan_bitmap.bmap );
/* this is unclear and lacks comments, explain how journal bitmaps
   work here for the reader.  Convey a sense of the design here. What
   is a window? */
/* - I mean `a window of zero bits' as in description of this function - Zam. */

    if ( !bi ) {
	printk("Hey, bitmap info pointer is zero for bitmap %d!\n",bmap_n);
	return 0;
    }
    if (buffer_locked (bi->bh)) {
       PROC_INFO_INC( s, scan_bitmap.wait );
       __wait_on_buffer (bi->bh);
    }

    /* If we know that first zero bit is only one or first zero bit is
       closer to the end of bitmap than our start pointer */
    if (bi->first_zero_hint > *beg || bi->free_count == 1)
	*beg = bi->first_zero_hint;

    while (1) {
	cont:
	if (bi->free_count < min)
		return 0; // No free blocks in this bitmap

	/* search for a first zero bit -- beggining of a window */
	*beg = reiserfs_find_next_zero_le_bit
	        ((unsigned long*)(bi->bh->b_data), boundary, *beg);

	if (*beg + min > boundary) { /* search for a zero bit fails or the rest of bitmap block
				      * cannot contain a zero window of minimum size */
	    return 0;
	}

	if (unfm && is_block_in_journal(s,bmap_n, *beg, beg))
	    continue;
	/* first zero bit found; we check next bits */
	for (end = *beg + 1;; end ++) {
	    if (end >= *beg + max || end >= boundary || reiserfs_test_le_bit (end, bi->bh->b_data)) {
		next = end;
		break;
	    }
	    /* finding the other end of zero bit window requires looking into journal structures (in
	     * case of searching for free blocks for unformatted nodes) */
	    if (unfm && is_block_in_journal(s, bmap_n, end, &next))
		break;
	}

	/* now (*beg) points to beginning of zero bits window,
	 * (end) points to one bit after the window end */
	if (end - *beg >= min) { /* it seems we have found window of proper size */
	    int i;
	    reiserfs_prepare_for_journal (s, bi->bh, 1);
	    /* try to set all blocks used checking are they still free */
	    for (i = *beg; i < end; i++) {
		/* It seems that we should not check in journal again. */
		if (reiserfs_test_and_set_le_bit (i, bi->bh->b_data)) {
		    /* bit was set by another process
		     * while we slept in prepare_for_journal() */
		    PROC_INFO_INC( s, scan_bitmap.stolen );
		    if (i >= *beg + min)	{ /* we can continue with smaller set of allocated blocks,
					   * if length of this set is more or equal to `min' */
			end = i;
			break;
		    }
		    /* otherwise we clear all bit were set ... */
		    while (--i >= *beg)
			reiserfs_test_and_clear_le_bit (i, bi->bh->b_data);
		    reiserfs_restore_prepared_buffer (s, bi->bh);
		    *beg = max(org, (int)bi->first_zero_hint);
		    /* ... and search again in current block from beginning */
		    goto cont;	
		}
	    }
	    bi->free_count -= (end - *beg);

	    /* if search started from zero_hint bit, and zero hint have not
                changed since, then we need to update first_zero_hint */
	    if ( bi->first_zero_hint >= *beg)
		/* no point in looking for free bit if there is not any */
		bi->first_zero_hint = (bi->free_count > 0 ) ?
			reiserfs_find_next_zero_le_bit
			((unsigned long*)(bi->bh->b_data), s->s_blocksize << 3, end) : (s->s_blocksize << 3);

	    journal_mark_dirty (th, s, bi->bh);

	    /* free block count calculation */
	    reiserfs_prepare_for_journal (s, SB_BUFFER_WITH_SB(s), 1);
	    PUT_SB_FREE_BLOCKS(s, SB_FREE_BLOCKS(s) - (end - *beg));
	    journal_mark_dirty (th, s, SB_BUFFER_WITH_SB(s));

	    return end - (*beg);
	} else {
	    *beg = next;
	}
    }
}

/* Tries to find contiguous zero bit window (given size) in given region of
 * bitmap and place new blocks there. Returns number of allocated blocks. */
static int scan_bitmap (struct reiserfs_transaction_handle *th,
			unsigned long *start, unsigned long finish,
			int min, int max, int unfm, unsigned long file_block)
{
    int nr_allocated=0;
    struct super_block * s = th->t_super;
    /* find every bm and bmap and bmap_nr in this file, and change them all to bitmap_blocknr
     * - Hans, it is not a block number - Zam. */

    int bm, off;
    int end_bm, end_off;
    int off_max = s->s_blocksize << 3;

    PROC_INFO_INC( s, scan_bitmap.call ); 
    if ( SB_FREE_BLOCKS(s) <= 0)
	return 0; // No point in looking for more free blocks

    get_bit_address (s, *start, &bm, &off);
    get_bit_address (s, finish, &end_bm, &end_off);

    // With this option set first we try to find a bitmap that is at least 10%
    // free, and if that fails, then we fall back to old whole bitmap scanning
    if ( TEST_OPTION(skip_busy, s) && SB_FREE_BLOCKS(s) > SB_BLOCK_COUNT(s)/20 ) {
	for (;bm < end_bm; bm++, off = 0) {
	    if ( ( off && (!unfm || (file_block != 0))) || SB_AP_BITMAP(s)[bm].free_count > (s->s_blocksize << 3) / 10 )
		nr_allocated = scan_bitmap_block(th, bm, &off, off_max, min, max, unfm);
	    if (nr_allocated)
		goto ret;
        }
	get_bit_address (s, *start, &bm, &off);
    }

    for (;bm < end_bm; bm++, off = 0) {
	nr_allocated = scan_bitmap_block(th, bm, &off, off_max, min, max, unfm);
	if (nr_allocated)
	    goto ret;
    }

    nr_allocated = scan_bitmap_block(th, bm, &off, end_off + 1, min, max, unfm);

 ret:
    *start = bm * off_max + off;
    return nr_allocated;

}

static void _reiserfs_free_block (struct reiserfs_transaction_handle *th,
			  b_blocknr_t block)
{
    struct super_block * s = th->t_super;
    struct reiserfs_super_block * rs;
    struct buffer_head * sbh;
    struct reiserfs_bitmap_info *apbi;
    int nr, offset;

    PROC_INFO_INC( s, free_block );

    rs = SB_DISK_SUPER_BLOCK (s);
    sbh = SB_BUFFER_WITH_SB (s);
    apbi = SB_AP_BITMAP(s);
  
    get_bit_address (s, block, &nr, &offset);
  
    if (nr >= sb_bmap_nr (rs)) {
	reiserfs_warning (s, "vs-4075: reiserfs_free_block: "
			  "block %lu is out of range on %s\n",
			  block, bdevname(s->s_dev));
	return;
    }

    reiserfs_prepare_for_journal(s, apbi[nr].bh, 1 ) ;
  
    /* clear bit for the given block in bit map */
    if (!reiserfs_test_and_clear_le_bit (offset, apbi[nr].bh->b_data)) {
	reiserfs_warning (s, "vs-4080: reiserfs_free_block: "
			  "free_block (%04x:%lu)[dev:blocknr]: bit already cleared\n", 
			  s->s_dev, block);
    }
    if (offset < apbi[nr].first_zero_hint) {
	apbi[nr].first_zero_hint = offset;
    }
    apbi[nr].free_count ++;
    journal_mark_dirty (th, s, apbi[nr].bh);
  
    reiserfs_prepare_for_journal(s, sbh, 1) ;
    /* update super block */
    set_sb_free_blocks( rs, sb_free_blocks(rs) + 1 );
  
    journal_mark_dirty (th, s, sbh);
}

void reiserfs_free_block (struct reiserfs_transaction_handle *th,
			  unsigned long block) {
    struct super_block * s = th->t_super;

    RFALSE(!s, "vs-4061: trying to free block on nonexistent device");
    RFALSE(is_reusable (s, block, 1) == 0, "vs-4071: can not free such block");
    /* mark it before we clear it, just in case */
    journal_mark_freed(th, s, block) ;
    _reiserfs_free_block(th, block) ;
}

/* preallocated blocks don't need to be run through journal_mark_freed */
void reiserfs_free_prealloc_block (struct reiserfs_transaction_handle *th, 
                          unsigned long block) {
    RFALSE(!th->t_super, "vs-4060: trying to free block on nonexistent device");
    RFALSE(is_reusable (th->t_super, block, 1) == 0, "vs-4070: can not free such block");
    _reiserfs_free_block(th, block) ;
}

static void __discard_prealloc (struct reiserfs_transaction_handle * th,
				struct inode * inode)
{
    unsigned long save = inode->u.reiserfs_i.i_prealloc_block ;
#ifdef CONFIG_REISERFS_CHECK
    if (inode->u.reiserfs_i.i_prealloc_count < 0)
	reiserfs_warning(th->t_super, "zam-4001:%s: inode has negative prealloc blocks count.\n", __FUNCTION__ );
#endif  
    while (inode->u.reiserfs_i.i_prealloc_count > 0) {
	reiserfs_free_prealloc_block(th,inode->u.reiserfs_i.i_prealloc_block);
	inode->u.reiserfs_i.i_prealloc_block++;
	inode->u.reiserfs_i.i_prealloc_count --;
    }
    inode->u.reiserfs_i.i_prealloc_block = save ;
    list_del (&(inode->u.reiserfs_i.i_prealloc_list));
}

/* FIXME: It should be inline function */
void reiserfs_discard_prealloc (struct reiserfs_transaction_handle *th,
				struct inode * inode)
{
    if (inode->u.reiserfs_i.i_prealloc_count) {
	__discard_prealloc(th, inode);
    }
}

void reiserfs_discard_all_prealloc (struct reiserfs_transaction_handle *th)
{
  struct list_head * plist = &SB_JOURNAL(th->t_super)->j_prealloc_list;
  struct inode * inode;
  
  while (!list_empty(plist)) {
    inode = list_entry(plist->next, struct inode, u.reiserfs_i.i_prealloc_list);
#ifdef CONFIG_REISERFS_CHECK
    if (!inode->u.reiserfs_i.i_prealloc_count) {
      reiserfs_warning(th->t_super, "zam-4001:%s: inode is in prealloc list but has no preallocated blocks.\n", __FUNCTION__ );
    }
#endif
    __discard_prealloc(th, inode);
  }
}

/* block allocator related options are parsed here */
int reiserfs_parse_alloc_options(struct super_block * s, char * options)
{
    char * this_char, * value;

    s->u.reiserfs_sb.s_alloc_options.bits = 0; /* clear default settings */

    for (this_char = strtok (options, ":"); this_char != NULL; this_char = strtok (NULL, ":")) {
	if ((value = strchr (this_char, '=')) != NULL)
	    *value++ = 0;

	if (!strcmp(this_char, "concentrating_formatted_nodes")) {
	    int temp;
	    SET_OPTION(concentrating_formatted_nodes);
	    temp = (value && *value) ? simple_strtoul (value, &value, 0) : 10;
	    if (temp <= 0 || temp > 100) {
		s->u.reiserfs_sb.s_alloc_options.border = 10;
	    } else {
		s->u.reiserfs_sb.s_alloc_options.border = 100 / temp;
	   }
	    continue;
	}
	if (!strcmp(this_char, "displacing_large_files")) {
	    SET_OPTION(displacing_large_files);
	    s->u.reiserfs_sb.s_alloc_options.large_file_size =
		(value && *value) ? simple_strtoul (value, &value, 0) : 16;
	    continue;
	}
	if (!strcmp(this_char, "displacing_new_packing_localities")) {
	    SET_OPTION(displacing_new_packing_localities);
	    continue;
	};

	if (!strcmp(this_char, "old_hashed_relocation")) {
	    SET_OPTION(old_hashed_relocation);
	    continue;
	}

	if (!strcmp(this_char, "new_hashed_relocation")) {
	    SET_OPTION(new_hashed_relocation);
	    continue;
	}

	if (!strcmp(this_char, "hashed_formatted_nodes")) {
	    SET_OPTION(hashed_formatted_nodes);
	    continue;
	}

	if (!strcmp(this_char, "skip_busy")) {
	    SET_OPTION(skip_busy);
	    continue;
	}

	if (!strcmp(this_char, "hundredth_slices")) {
	    SET_OPTION(hundredth_slices);
	    continue;
	}

	if (!strcmp(this_char, "old_way")) {
	    SET_OPTION(old_way);
	    continue;
	}

	if (!strcmp(this_char, "displace_based_on_dirid")) {
	    SET_OPTION(displace_based_on_dirid);
	    continue;
	}

	if (!strcmp(this_char, "preallocmin")) {
	    s->u.reiserfs_sb.s_alloc_options.preallocmin =
		(value && *value) ? simple_strtoul (value, &value, 0) : 4;
	    continue;
	}

	if (!strcmp(this_char, "preallocsize")) {
	    s->u.reiserfs_sb.s_alloc_options.preallocsize =
		(value && *value) ? simple_strtoul (value, &value, 0) : PREALLOCATION_SIZE;
	    continue;
	}

	reiserfs_warning(s, "zam-4001: %s : unknown option - %s\n", __FUNCTION__ , this_char);
	return 1;
    }

    return 0;
}

static void inline new_hashed_relocation (reiserfs_blocknr_hint_t * hint)
{
    char * hash_in;
    if (hint->formatted_node) {
	    hash_in = (char*)&hint->key.k_dir_id;
    } else {
	if (!hint->inode) {
	    //hint->search_start = hint->beg;
	    hash_in = (char*)&hint->key.k_dir_id;
	} else 
	    if ( TEST_OPTION(displace_based_on_dirid, hint->th->t_super))
		hash_in = (char *)(&INODE_PKEY(hint->inode)->k_dir_id);
	    else
		hash_in = (char *)(&INODE_PKEY(hint->inode)->k_objectid);
    }

    hint->search_start = hint->beg + keyed_hash(hash_in, 4) % (hint->end - hint->beg);
}

static void inline get_left_neighbor(reiserfs_blocknr_hint_t *hint)
{
    struct path * path;
    struct buffer_head * bh;
    struct item_head * ih;
    int pos_in_item;
    __u32 * item;

    if (!hint->path)		/* reiserfs code can call this function w/o pointer to path
				 * structure supplied; then we rely on supplied search_start */
	return;

    path = hint->path;
    bh = get_last_bh(path);
    RFALSE( !bh, "green-4002: Illegal path specified to get_left_neighbor\n");
    ih = get_ih(path);
    pos_in_item = path->pos_in_item;
    item = get_item (path);

    hint->search_start = bh->b_blocknr;

    if (!hint->formatted_node && is_indirect_le_ih (ih)) {
	/* for indirect item: go to left and look for the first non-hole entry
	   in the indirect item */
	if (pos_in_item == I_UNFM_NUM (ih))
	    pos_in_item--;
//	    pos_in_item = I_UNFM_NUM (ih) - 1;
	while (pos_in_item >= 0) {
	    int t=get_block_num(item,pos_in_item);
	    if (t) {
		hint->search_start = t;
		break;
	    }
	    pos_in_item --;
	}
    } else {
    }

    /* does result value fit into specified region? */
    return;
}

/* should be, if formatted node, then try to put on first part of the device
   specified as number of percent with mount option device, else try to put
   on last of device.  This is not to say it is good code to do so,
   but the effect should be measured.  */
static void inline set_border_in_hint(struct super_block *s, reiserfs_blocknr_hint_t *hint)
{
    b_blocknr_t border = SB_BLOCK_COUNT(hint->th->t_super) / s->u.reiserfs_sb.s_alloc_options.border;

    if (hint->formatted_node)
	hint->end = border - 1;
    else
	hint->beg = border;
}

static void inline displace_large_file(reiserfs_blocknr_hint_t *hint)
{
    if ( TEST_OPTION(displace_based_on_dirid, hint->th->t_super))
	hint->search_start = hint->beg + keyed_hash((char *)(&INODE_PKEY(hint->inode)->k_dir_id), 4) % (hint->end - hint->beg);
    else
	hint->search_start = hint->beg + keyed_hash((char *)(&INODE_PKEY(hint->inode)->k_objectid), 4) % (hint->end - hint->beg);
}

static void inline hash_formatted_node(reiserfs_blocknr_hint_t *hint)
{
   char * hash_in;

   if (!hint->inode)
	hash_in = (char*)&hint->key.k_dir_id;
    else if ( TEST_OPTION(displace_based_on_dirid, hint->th->t_super))
	hash_in = (char *)(&INODE_PKEY(hint->inode)->k_dir_id);
    else
	hash_in = (char *)(&INODE_PKEY(hint->inode)->k_objectid);

	hint->search_start = hint->beg + keyed_hash(hash_in, 4) % (hint->end - hint->beg);
}

static int inline this_blocknr_allocation_would_make_it_a_large_file(reiserfs_blocknr_hint_t *hint)
{
    return hint->block == hint->th->t_super->u.reiserfs_sb.s_alloc_options.large_file_size;
}

#ifdef DISPLACE_NEW_PACKING_LOCALITIES
static void inline displace_new_packing_locality (reiserfs_blocknr_hint_t *hint)
{
    struct key * key = &hint->key;

    hint->th->displace_new_blocks = 0;
    hint->search_start = hint->beg + keyed_hash((char*)(&key->k_objectid),4) % (hint->end - hint->beg);
}
#endif

static int inline old_hashed_relocation (reiserfs_blocknr_hint_t * hint)
{
    unsigned long border;
    unsigned long hash_in;
    
    if (hint->formatted_node || hint->inode == NULL) {
	return 0;
    }

    hash_in = le32_to_cpu((INODE_PKEY(hint->inode))->k_dir_id);
    border = hint->beg + (unsigned long) keyed_hash(((char *) (&hash_in)), 4) % (hint->end - hint->beg - 1);
    if (border > hint->search_start)
	hint->search_start = border;

    return 1;
}

static int inline old_way (reiserfs_blocknr_hint_t * hint)
{
    unsigned long border;
    
    if (hint->formatted_node || hint->inode == NULL) {
	return 0;
    }

      border = hint->beg + le32_to_cpu(INODE_PKEY(hint->inode)->k_dir_id) % (hint->end  - hint->beg);
    if (border > hint->search_start)
	hint->search_start = border;

    return 1;
}

static void inline hundredth_slices (reiserfs_blocknr_hint_t * hint)
{
    struct key * key = &hint->key;
    unsigned long slice_start;

    slice_start = (keyed_hash((char*)(&key->k_dir_id),4) % 100) * (hint->end / 100);
    if ( slice_start > hint->search_start || slice_start + (hint->end / 100) <= hint->search_start) {
	hint->search_start = slice_start;
    }
}

static void inline determine_search_start(reiserfs_blocknr_hint_t *hint,
					  int amount_needed)
{
    struct super_block *s = hint->th->t_super;
    hint->beg = 0;
    hint->end = SB_BLOCK_COUNT(s) - 1;

    /* This is former border algorithm. Now with tunable border offset */
    if (concentrating_formatted_nodes(s))
	set_border_in_hint(s, hint);

#ifdef DISPLACE_NEW_PACKING_LOCALITIES
    /* whenever we create a new directory, we displace it.  At first we will
       hash for location, later we might look for a moderately empty place for
       it */
    if (displacing_new_packing_localities(s)
	&& hint->th->displace_new_blocks) {
	displace_new_packing_locality(hint);

	/* we do not continue determine_search_start,
	 * if new packing locality is being displaced */
	return;
    }				      
#endif

    /* all persons should feel encouraged to add more special cases here and
     * test them */

    if (displacing_large_files(s) && !hint->formatted_node
	&& this_blocknr_allocation_would_make_it_a_large_file(hint)) {
	displace_large_file(hint);
	return;
    }

    /* attempt to copy a feature from old block allocator code */
    if (TEST_OPTION(old_hashed_relocation, s) && !hint->formatted_node) {
	old_hashed_relocation(hint);
    }

    /* if none of our special cases is relevant, use the left neighbor in the
       tree order of the new node we are allocating for */
    if (hint->formatted_node && TEST_OPTION(hashed_formatted_nodes,s)) {
	hash_formatted_node(hint);
	return;
    } 

    get_left_neighbor(hint);

    /* Mimic old block allocator behaviour, that is if VFS allowed for preallocation,
       new blocks are displaced based on directory ID. Also, if suggested search_start
       is less than last preallocated block, we start searching from it, assuming that
       HDD dataflow is faster in forward direction */
    if ( TEST_OPTION(old_way, s)) {
	if (!hint->formatted_node) {
	    if ( !reiserfs_hashed_relocation(s))
		old_way(hint);
	    else if (!reiserfs_no_unhashed_relocation(s))
		old_hashed_relocation(hint);

	    if ( hint->inode && hint->search_start < hint->inode->u.reiserfs_i.i_prealloc_block)
		hint->search_start = hint->inode->u.reiserfs_i.i_prealloc_block;
	}
	return;
    }

    /* This is an approach proposed by Hans */
    if ( TEST_OPTION(hundredth_slices, s) && ! (displacing_large_files(s) && !hint->formatted_node)) {
	hundredth_slices(hint);
	return;
    }

    if (TEST_OPTION(old_hashed_relocation, s))
	old_hashed_relocation(hint);
    if (TEST_OPTION(new_hashed_relocation, s))
	new_hashed_relocation(hint);
    return;
}

static int determine_prealloc_size(reiserfs_blocknr_hint_t * hint)
{
    /* make minimum size a mount option and benchmark both ways */
    /* we preallocate blocks only for regular files, specific size */
    /* benchmark preallocating always and see what happens */

    hint->prealloc_size = 0;

    if (!hint->formatted_node && hint->preallocate) {
	if (S_ISREG(hint->inode->i_mode)
	    && hint->inode->i_size >= hint->th->t_super->u.reiserfs_sb.s_alloc_options.preallocmin * hint->inode->i_sb->s_blocksize)
	    hint->prealloc_size = hint->th->t_super->u.reiserfs_sb.s_alloc_options.preallocsize - 1;
    }
    return CARRY_ON;
}

/* XXX I know it could be merged with upper-level function;
   but may be result function would be too complex. */
static inline int allocate_without_wrapping_disk (reiserfs_blocknr_hint_t * hint,
					 b_blocknr_t * new_blocknrs,
					 b_blocknr_t start, b_blocknr_t finish,
					 int amount_needed, int prealloc_size)
{
    int rest = amount_needed;
    int nr_allocated;

    while (rest > 0) {
	nr_allocated = scan_bitmap (hint->th, &start, finish, 1,
				    rest + prealloc_size, !hint->formatted_node,
				    hint->block);

	if (nr_allocated == 0)	/* no new blocks allocated, return */
	    break;
	
	/* fill free_blocknrs array first */
	while (rest > 0 && nr_allocated > 0) {
	    * new_blocknrs ++ = start ++;
	    rest --; nr_allocated --;
	}

	/* do we have something to fill prealloc. array also ? */
	if (nr_allocated > 0) {
	    /* it means prealloc_size was greater that 0 and we do preallocation */
	    list_add(&INODE_INFO(hint->inode)->i_prealloc_list,
		     &SB_JOURNAL(hint->th->t_super)->j_prealloc_list);
	    INODE_INFO(hint->inode)->i_prealloc_block = start;
	    INODE_INFO(hint->inode)->i_prealloc_count = nr_allocated;
	    break;
	}
    }

    return (amount_needed - rest);
}

static inline int blocknrs_and_prealloc_arrays_from_search_start
    (reiserfs_blocknr_hint_t *hint, b_blocknr_t *new_blocknrs, int amount_needed)
{
    struct super_block *s = hint->th->t_super;
    b_blocknr_t start = hint->search_start;
    b_blocknr_t finish = SB_BLOCK_COUNT(s) - 1;
    int second_pass = 0;
    int nr_allocated = 0;

    determine_prealloc_size(hint);
    while((nr_allocated
	  += allocate_without_wrapping_disk(hint, new_blocknrs + nr_allocated, start, finish,
					  amount_needed - nr_allocated, hint->prealloc_size))
	  < amount_needed) {

	/* not all blocks were successfully allocated yet*/
	if (second_pass) {	/* it was a second pass; we must free all blocks */
	    while (nr_allocated --)
		reiserfs_free_block(hint->th, new_blocknrs[nr_allocated]);

	    return NO_DISK_SPACE;
	} else {		/* refine search parameters for next pass */
	    second_pass = 1;
	    finish = start;
	    start = 0;
	    continue;
	}
    }
    return CARRY_ON;
}

/* grab new blocknrs from preallocated list */
/* return amount still needed after using them */
static int use_preallocated_list_if_available (reiserfs_blocknr_hint_t *hint,
					       b_blocknr_t *new_blocknrs, int amount_needed)
{
    struct inode * inode = hint->inode;

    if (INODE_INFO(inode)->i_prealloc_count > 0) {
	while (amount_needed) {

	    *new_blocknrs ++ = INODE_INFO(inode)->i_prealloc_block ++;
	    INODE_INFO(inode)->i_prealloc_count --;

	    amount_needed --;

	    if (INODE_INFO(inode)->i_prealloc_count <= 0) {
		list_del(&inode->u.reiserfs_i.i_prealloc_list);  
		break;
	    }
	}
    }
    /* return amount still needed after using preallocated blocks */
    return amount_needed;
}

int reiserfs_allocate_blocknrs(reiserfs_blocknr_hint_t *hint,
			       b_blocknr_t * new_blocknrs, int amount_needed,
			       int reserved_by_us /* Amount of blocks we have
						      already reserved */)
{
    int initial_amount_needed = amount_needed;
    int ret;

    /* Check if there is enough space, taking into account reserved space */
    if ( SB_FREE_BLOCKS(hint->th->t_super) - hint->th->t_super->u.reiserfs_sb.reserved_blocks <
	 amount_needed - reserved_by_us)
        return NO_DISK_SPACE;
    /* should this be if !hint->inode &&  hint->preallocate? */
    /* do you mean hint->formatted_node can be removed ? - Zam */
    /* hint->formatted_node cannot be removed because we try to access
       inode information here, and there is often no inode assotiated with
       metadata allocations - green */

    if (!hint->formatted_node && hint->preallocate) {
	amount_needed = use_preallocated_list_if_available
	    (hint, new_blocknrs, amount_needed);
	if (amount_needed == 0)	/* all blocknrs we need we got from
                                   prealloc. list */
	    return CARRY_ON;
	new_blocknrs += (initial_amount_needed - amount_needed);
    }

    /* find search start and save it in hint structure */
    determine_search_start(hint, amount_needed);

    /* allocation itself; fill new_blocknrs and preallocation arrays */
    ret = blocknrs_and_prealloc_arrays_from_search_start
	(hint, new_blocknrs, amount_needed);

    /* we used prealloc. list to fill (partially) new_blocknrs array. If final allocation fails we
     * need to return blocks back to prealloc. list or just free them. -- Zam (I chose second
     * variant) */

    if (ret != CARRY_ON) {
	while (amount_needed ++ < initial_amount_needed) {
	    reiserfs_free_block(hint->th, *(--new_blocknrs));
	}
    }
    return ret;
}

/* These 2 functions are here to provide blocks reservation to the rest of kernel */
/* Reserve @blocks amount of blocks in fs pointed by @sb. Caller must make sure
   there are actually this much blocks on the FS available */
void reiserfs_claim_blocks_to_be_allocated( 
				      struct super_block *sb, /* super block of
							        filesystem where
								blocks should be
								reserved */
				      int blocks /* How much to reserve */
					  )
{

    /* Fast case, if reservation is zero - exit immediately. */
    if ( !blocks )
	return;

    sb->u.reiserfs_sb.reserved_blocks += blocks;
}

/* Unreserve @blocks amount of blocks in fs pointed by @sb */
void reiserfs_release_claimed_blocks( 
				struct super_block *sb, /* super block of
							  filesystem where
							  blocks should be
							  reserved */
				int blocks /* How much to unreserve */
					  )
{

    /* Fast case, if unreservation is zero - exit immediately. */
    if ( !blocks )
	return;

    sb->u.reiserfs_sb.reserved_blocks -= blocks;
    RFALSE( sb->u.reiserfs_sb.reserved_blocks < 0, "amount of blocks reserved became zero?");
}
