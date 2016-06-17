/*
 *  Copyright 2000-2002 by Hans Reiser, licensing governed by reiserfs/README
 */

/*
 *  Written by Anatoly P. Pinchuk pap@namesys.botik.ru
 *  Programm System Institute
 *  Pereslavl-Zalessky Russia
 */

/*
 *  This file contains functions dealing with S+tree
 *
 * B_IS_IN_TREE
 * copy_short_key
 * copy_item_head
 * comp_short_keys
 * comp_keys
 * comp_cpu_keys
 * comp_short_le_keys
 * comp_short_cpu_keys
 * cpu_key2cpu_key
 * le_key2cpu_key
 * comp_le_keys
 * bin_search
 * get_lkey
 * get_rkey
 * key_in_buffer
 * decrement_bcount
 * decrement_counters_in_path
 * reiserfs_check_path
 * pathrelse_and_restore
 * pathrelse
 * search_by_key_reada
 * search_by_key
 * search_for_position_by_key
 * comp_items
 * prepare_for_direct_item
 * prepare_for_direntry_item
 * prepare_for_delete_or_cut
 * calc_deleted_bytes_number
 * init_tb_struct
 * padd_item
 * reiserfs_delete_item
 * reiserfs_delete_solid_item
 * reiserfs_delete_object
 * maybe_indirect_to_direct
 * indirect_to_direct_roll_back
 * reiserfs_cut_from_item
 * truncate_directory
 * reiserfs_do_truncate
 * reiserfs_paste_into_item
 * reiserfs_insert_item
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/pagemap.h>
#include <linux/reiserfs_fs.h>
#include <linux/smp_lock.h>

/* Does the buffer contain a disk block which is in the tree. */
inline int B_IS_IN_TREE (const struct buffer_head * p_s_bh)
{

  RFALSE( B_LEVEL (p_s_bh) > MAX_HEIGHT,
	  "PAP-1010: block (%b) has too big level (%z)", p_s_bh, p_s_bh);

  return ( B_LEVEL (p_s_bh) != FREE_LEVEL );
}




inline void copy_short_key (void * to, const void * from)
{
    memcpy (to, from, SHORT_KEY_SIZE);
}

//
// to gets item head in le form
//
inline void copy_item_head(struct item_head * p_v_to, 
			   const struct item_head * p_v_from)
{
  memcpy (p_v_to, p_v_from, IH_SIZE);
}


/* k1 is pointer to on-disk structure which is stored in little-endian
   form. k2 is pointer to cpu variable. For key of items of the same
   object this returns 0.
   Returns: -1 if key1 < key2 
   0 if key1 == key2
   1 if key1 > key2 */
inline int  comp_short_keys (const struct key * le_key, 
			     const struct cpu_key * cpu_key)
{
  __u32 * p_s_le_u32, * p_s_cpu_u32;
  int n_key_length = REISERFS_SHORT_KEY_LEN;

  p_s_le_u32 = (__u32 *)le_key;
  p_s_cpu_u32 = (__u32 *)cpu_key;
  for( ; n_key_length--; ++p_s_le_u32, ++p_s_cpu_u32 ) {
    if ( le32_to_cpu (*p_s_le_u32) < *p_s_cpu_u32 )
      return -1;
    if ( le32_to_cpu (*p_s_le_u32) > *p_s_cpu_u32 )
      return 1;
  }

  return 0;
}


/* k1 is pointer to on-disk structure which is stored in little-endian
   form. k2 is pointer to cpu variable.
   Compare keys using all 4 key fields.
   Returns: -1 if key1 < key2 0
   if key1 = key2 1 if key1 > key2 */
inline int  comp_keys (const struct key * le_key, const struct cpu_key * cpu_key)
{
  int retval;

  retval = comp_short_keys (le_key, cpu_key);
  if (retval)
      return retval;
  if (le_key_k_offset (le_key_version(le_key), le_key) < cpu_key_k_offset (cpu_key))
      return -1;
  if (le_key_k_offset (le_key_version(le_key), le_key) > cpu_key_k_offset (cpu_key))
      return 1;

  if (cpu_key->key_length == 3)
      return 0;

  /* this part is needed only when tail conversion is in progress */
  if (le_key_k_type (le_key_version(le_key), le_key) < cpu_key_k_type (cpu_key))
    return -1;

  if (le_key_k_type (le_key_version(le_key), le_key) > cpu_key_k_type (cpu_key))
    return 1;

  return 0;
}


//
// FIXME: not used yet
//
inline int comp_cpu_keys (const struct cpu_key * key1, 
			  const struct cpu_key * key2)
{
    if (key1->on_disk_key.k_dir_id < key2->on_disk_key.k_dir_id)
	return -1;
    if (key1->on_disk_key.k_dir_id > key2->on_disk_key.k_dir_id)
	return 1;

    if (key1->on_disk_key.k_objectid < key2->on_disk_key.k_objectid)
	return -1;
    if (key1->on_disk_key.k_objectid > key2->on_disk_key.k_objectid)
	return 1;

    if (cpu_key_k_offset (key1) < cpu_key_k_offset (key2))
	return -1;
    if (cpu_key_k_offset (key1) > cpu_key_k_offset (key2))
	return 1;

    reiserfs_warning (NULL, "comp_cpu_keys: type are compared for %K and %K\n",
		      key1, key2);

    if (cpu_key_k_type (key1) < cpu_key_k_type (key2))
	return -1;
    if (cpu_key_k_type (key1) > cpu_key_k_type (key2))
	return 1;
    return 0;
}

inline int comp_short_le_keys (const struct key * key1, const struct key * key2)
{
  __u32 * p_s_1_u32, * p_s_2_u32;
  int n_key_length = REISERFS_SHORT_KEY_LEN;

  p_s_1_u32 = (__u32 *)key1;
  p_s_2_u32 = (__u32 *)key2;
  for( ; n_key_length--; ++p_s_1_u32, ++p_s_2_u32 ) {
    if ( le32_to_cpu (*p_s_1_u32) < le32_to_cpu (*p_s_2_u32) )
      return -1;
    if ( le32_to_cpu (*p_s_1_u32) > le32_to_cpu (*p_s_2_u32) )
      return 1;
  }
  return 0;
}

inline int comp_short_cpu_keys (const struct cpu_key * key1, 
				const struct cpu_key * key2)
{
  __u32 * p_s_1_u32, * p_s_2_u32;
  int n_key_length = REISERFS_SHORT_KEY_LEN;

  p_s_1_u32 = (__u32 *)key1;
  p_s_2_u32 = (__u32 *)key2;

  for( ; n_key_length--; ++p_s_1_u32, ++p_s_2_u32 ) {
    if ( *p_s_1_u32 < *p_s_2_u32 )
      return -1;
    if ( *p_s_1_u32 > *p_s_2_u32 )
      return 1;
  }
  return 0;
}



inline void cpu_key2cpu_key (struct cpu_key * to, const struct cpu_key * from)
{
    memcpy (to, from, sizeof (struct cpu_key));
}


inline void le_key2cpu_key (struct cpu_key * to, const struct key * from)
{
    to->on_disk_key.k_dir_id = le32_to_cpu (from->k_dir_id);
    to->on_disk_key.k_objectid = le32_to_cpu (from->k_objectid);
    
    // find out version of the key
    to->version = le_key_version (from);
    if (to->version == KEY_FORMAT_3_5) {
	to->on_disk_key.u.k_offset_v1.k_offset = le32_to_cpu (from->u.k_offset_v1.k_offset);
	to->on_disk_key.u.k_offset_v1.k_uniqueness = le32_to_cpu (from->u.k_offset_v1.k_uniqueness);
    } else {
	to->on_disk_key.u.k_offset_v2.k_offset = offset_v2_k_offset(&from->u.k_offset_v2);
	to->on_disk_key.u.k_offset_v2.k_type = offset_v2_k_type(&from->u.k_offset_v2);
    } 
}



// this does not say which one is bigger, it only returns 1 if keys
// are not equal, 0 otherwise
inline int comp_le_keys (const struct key * k1, const struct key * k2)
{
    return memcmp (k1, k2, sizeof (struct key));
}

/**************************************************************************
 *  Binary search toolkit function                                        *
 *  Search for an item in the array by the item key                       *
 *  Returns:    1 if found,  0 if not found;                              *
 *        *p_n_pos = number of the searched element if found, else the    *
 *        number of the first element that is larger than p_v_key.        *
 **************************************************************************/
/* For those not familiar with binary search: n_lbound is the leftmost item that it
 could be, n_rbound the rightmost item that it could be.  We examine the item
 halfway between n_lbound and n_rbound, and that tells us either that we can increase
 n_lbound, or decrease n_rbound, or that we have found it, or if n_lbound <= n_rbound that
 there are no possible items, and we have not found it. With each examination we
 cut the number of possible items it could be by one more than half rounded down,
 or we find it. */
inline	int bin_search (
              const void * p_v_key, /* Key to search for.                   */
	      const void * p_v_base,/* First item in the array.             */
	      int       p_n_num,    /* Number of items in the array.        */
	      int       p_n_width,  /* Item size in the array.
				       searched. Lest the reader be
				       confused, note that this is crafted
				       as a general function, and when it
				       is applied specifically to the array
				       of item headers in a node, p_n_width
				       is actually the item header size not
				       the item size.                      */
	      int     * p_n_pos     /* Number of the searched for element. */
            ) {
    int   n_rbound, n_lbound, n_j;

   for ( n_j = ((n_rbound = p_n_num - 1) + (n_lbound = 0))/2; n_lbound <= n_rbound; n_j = (n_rbound + n_lbound)/2 )
     switch( COMP_KEYS((struct key *)((char * )p_v_base + n_j * p_n_width), (struct cpu_key *)p_v_key) )  {
     case -1: n_lbound = n_j + 1; continue;
     case  1: n_rbound = n_j - 1; continue;
     case  0: *p_n_pos = n_j;     return ITEM_FOUND; /* Key found in the array.  */
        }

    /* bin_search did not find given key, it returns position of key,
        that is minimal and greater than the given one. */
    *p_n_pos = n_lbound;
    return ITEM_NOT_FOUND;
}

#ifdef CONFIG_REISERFS_CHECK
extern struct tree_balance * cur_tb;
#endif



/* Minimal possible key. It is never in the tree. */
const struct key  MIN_KEY = {0, 0, {{0, 0},}};

/* Maximal possible key. It is never in the tree. */
const struct key  MAX_KEY = {0xffffffff, 0xffffffff, {{0xffffffff, 0xffffffff},}};


/* Get delimiting key of the buffer by looking for it in the buffers in the path, starting from the bottom
   of the path, and going upwards.  We must check the path's validity at each step.  If the key is not in
   the path, there is no delimiting key in the tree (buffer is first or last buffer in tree), and in this
   case we return a special key, either MIN_KEY or MAX_KEY. */
inline	const struct  key * get_lkey  (
	                const struct path         * p_s_chk_path,
                        const struct super_block  * p_s_sb
                      ) {
  int                   n_position, n_path_offset = p_s_chk_path->path_length;
  struct buffer_head  * p_s_parent;
  
  RFALSE( n_path_offset < FIRST_PATH_ELEMENT_OFFSET, 
	  "PAP-5010: illegal offset in the path");

  /* While not higher in path than first element. */
  while ( n_path_offset-- > FIRST_PATH_ELEMENT_OFFSET ) {

    RFALSE( ! buffer_uptodate(PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset)),
	    "PAP-5020: parent is not uptodate");

    /* Parent at the path is not in the tree now. */
    if ( ! B_IS_IN_TREE(p_s_parent = PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset)) )
      return &MAX_KEY;
    /* Check whether position in the parent is correct. */
    if ( (n_position = PATH_OFFSET_POSITION(p_s_chk_path, n_path_offset)) > B_NR_ITEMS(p_s_parent) )
       return &MAX_KEY;
    /* Check whether parent at the path really points to the child. */
    if ( B_N_CHILD_NUM(p_s_parent, n_position) !=
	 PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset + 1)->b_blocknr )
      return &MAX_KEY;
    /* Return delimiting key if position in the parent is not equal to zero. */
    if ( n_position )
      return B_N_PDELIM_KEY(p_s_parent, n_position - 1);
  }
  /* Return MIN_KEY if we are in the root of the buffer tree. */
  if ( PATH_OFFSET_PBUFFER(p_s_chk_path, FIRST_PATH_ELEMENT_OFFSET)->b_blocknr ==
       SB_ROOT_BLOCK (p_s_sb) )
    return &MIN_KEY;
  return  &MAX_KEY;
}


/* Get delimiting key of the buffer at the path and its right neighbor. */
inline	const struct  key * get_rkey  (
	                const struct path         * p_s_chk_path,
                        const struct super_block  * p_s_sb
                      ) {
  int                   n_position,
    			n_path_offset = p_s_chk_path->path_length;
  struct buffer_head  * p_s_parent;

  RFALSE( n_path_offset < FIRST_PATH_ELEMENT_OFFSET,
	  "PAP-5030: illegal offset in the path");

  while ( n_path_offset-- > FIRST_PATH_ELEMENT_OFFSET ) {

    RFALSE( ! buffer_uptodate(PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset)),
	    "PAP-5040: parent is not uptodate");

    /* Parent at the path is not in the tree now. */
    if ( ! B_IS_IN_TREE(p_s_parent = PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset)) )
      return &MIN_KEY;
    /* Check whether position in the parent is correct. */
    if ( (n_position = PATH_OFFSET_POSITION(p_s_chk_path, n_path_offset)) > B_NR_ITEMS(p_s_parent) )
      return &MIN_KEY;
    /* Check whether parent at the path really points to the child. */
    if ( B_N_CHILD_NUM(p_s_parent, n_position) !=
                                        PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset + 1)->b_blocknr )
      return &MIN_KEY;
    /* Return delimiting key if position in the parent is not the last one. */
    if ( n_position != B_NR_ITEMS(p_s_parent) )
      return B_N_PDELIM_KEY(p_s_parent, n_position);
  }
  /* Return MAX_KEY if we are in the root of the buffer tree. */
  if ( PATH_OFFSET_PBUFFER(p_s_chk_path, FIRST_PATH_ELEMENT_OFFSET)->b_blocknr ==
       SB_ROOT_BLOCK (p_s_sb) )
    return &MAX_KEY;
  return  &MIN_KEY;
}


/* Check whether a key is contained in the tree rooted from a buffer at a path. */
/* This works by looking at the left and right delimiting keys for the buffer in the last path_element in
   the path.  These delimiting keys are stored at least one level above that buffer in the tree. If the
   buffer is the first or last node in the tree order then one of the delimiting keys may be absent, and in
   this case get_lkey and get_rkey return a special key which is MIN_KEY or MAX_KEY. */
static  inline  int key_in_buffer (
                      struct path         * p_s_chk_path, /* Path which should be checked.  */
                      const struct cpu_key      * p_s_key,      /* Key which should be checked.   */
                      struct super_block  * p_s_sb        /* Super block pointer.           */
		      ) {

  RFALSE( ! p_s_key || p_s_chk_path->path_length < FIRST_PATH_ELEMENT_OFFSET ||
	  p_s_chk_path->path_length > MAX_HEIGHT,
	  "PAP-5050: pointer to the key(%p) is NULL or illegal path length(%d)",
	  p_s_key, p_s_chk_path->path_length);
  RFALSE( PATH_PLAST_BUFFER(p_s_chk_path)->b_dev == NODEV,
	  "PAP-5060: device must not be NODEV");

  if ( COMP_KEYS(get_lkey(p_s_chk_path, p_s_sb), p_s_key) == 1 )
    /* left delimiting key is bigger, that the key we look for */
    return 0;
  //  if ( COMP_KEYS(p_s_key, get_rkey(p_s_chk_path, p_s_sb)) != -1 )
  if ( COMP_KEYS(get_rkey(p_s_chk_path, p_s_sb), p_s_key) != 1 )
    /* p_s_key must be less than right delimitiing key */
    return 0;
  return 1;
}


inline void decrement_bcount(
              struct buffer_head  * p_s_bh
            ) { 
  if ( p_s_bh ) {
    if ( atomic_read (&(p_s_bh->b_count)) ) {
      put_bh(p_s_bh) ;
      return;
    }
    reiserfs_panic(NULL, "PAP-5070: decrement_bcount: trying to free free buffer %b", p_s_bh);
  }
}


/* Decrement b_count field of the all buffers in the path. */
void decrement_counters_in_path (
              struct path * p_s_search_path
            ) {
  int n_path_offset = p_s_search_path->path_length;

  RFALSE( n_path_offset < ILLEGAL_PATH_ELEMENT_OFFSET ||
	  n_path_offset > EXTENDED_MAX_HEIGHT - 1,
	  "PAP-5080: illegal path offset of %d", n_path_offset);

  while ( n_path_offset > ILLEGAL_PATH_ELEMENT_OFFSET ) {
    struct buffer_head * bh;

    bh = PATH_OFFSET_PBUFFER(p_s_search_path, n_path_offset--);
    decrement_bcount (bh);
  }
  p_s_search_path->path_length = ILLEGAL_PATH_ELEMENT_OFFSET;
}


int reiserfs_check_path(struct path *p) {
  RFALSE( p->path_length != ILLEGAL_PATH_ELEMENT_OFFSET,
	  "path not properly relsed") ;
  return 0 ;
}


/* Release all buffers in the path. Restore dirty bits clean
** when preparing the buffer for the log
**
** only called from fix_nodes()
*/
void  pathrelse_and_restore (
	struct super_block *s, 
        struct path * p_s_search_path
      ) {
  int n_path_offset = p_s_search_path->path_length;

  RFALSE( n_path_offset < ILLEGAL_PATH_ELEMENT_OFFSET, 
	  "clm-4000: illegal path offset");
  
  while ( n_path_offset > ILLEGAL_PATH_ELEMENT_OFFSET )  {
    reiserfs_restore_prepared_buffer(s, PATH_OFFSET_PBUFFER(p_s_search_path, 
                                     n_path_offset));
    brelse(PATH_OFFSET_PBUFFER(p_s_search_path, n_path_offset--));
  }
  p_s_search_path->path_length = ILLEGAL_PATH_ELEMENT_OFFSET;
}

/* Release all buffers in the path. */
void  pathrelse (
        struct path * p_s_search_path
      ) {
  int n_path_offset = p_s_search_path->path_length;

  RFALSE( n_path_offset < ILLEGAL_PATH_ELEMENT_OFFSET,
	  "PAP-5090: illegal path offset");
  
  while ( n_path_offset > ILLEGAL_PATH_ELEMENT_OFFSET )  
    brelse(PATH_OFFSET_PBUFFER(p_s_search_path, n_path_offset--));

  p_s_search_path->path_length = ILLEGAL_PATH_ELEMENT_OFFSET;
}



static int is_leaf (char * buf, int blocksize, struct buffer_head * bh)
{
    struct block_head * blkh;
    struct item_head * ih;
    int used_space;
    int prev_location;
    int i;
    int nr;

    blkh = (struct block_head *)buf;
    if ( blkh_level(blkh) != DISK_LEAF_NODE_LEVEL) {
	printk ("is_leaf: this should be caught earlier\n");
	return 0;
    }

    nr = blkh_nr_item(blkh);
    if (nr < 1 || nr > ((blocksize - BLKH_SIZE) / (IH_SIZE + MIN_ITEM_LEN))) {
	/* item number is too big or too small */
	reiserfs_warning (NULL, "is_leaf: nr_item seems wrong: %z\n", bh);
	return 0;
    }
    ih = (struct item_head *)(buf + BLKH_SIZE) + nr - 1;
    used_space = BLKH_SIZE + IH_SIZE * nr + (blocksize - ih_location (ih));
    if (used_space != blocksize - blkh_free_space(blkh)) {
	/* free space does not match to calculated amount of use space */
	reiserfs_warning (NULL, "is_leaf: free space seems wrong: %z\n", bh);
	return 0;
    }

    // FIXME: it is_leaf will hit performance too much - we may have
    // return 1 here

    /* check tables of item heads */
    ih = (struct item_head *)(buf + BLKH_SIZE);
    prev_location = blocksize;
    for (i = 0; i < nr; i ++, ih ++) {
	if ( le_ih_k_type(ih) == TYPE_ANY) {
	    reiserfs_warning (NULL, "is_leaf: wrong item type for item %h\n",ih);
	    return 0;
	}
	if (ih_location (ih) >= blocksize || ih_location (ih) < IH_SIZE * nr) {
	    reiserfs_warning (NULL, "is_leaf: item location seems wrong: %h\n", ih);
	    return 0;
	}
	if (ih_item_len (ih) < 1 || ih_item_len (ih) > MAX_ITEM_LEN (blocksize)) {
	    reiserfs_warning (NULL, "is_leaf: item length seems wrong: %h\n", ih);
	    return 0;
	}
	if (prev_location - ih_location (ih) != ih_item_len (ih)) {
	    reiserfs_warning (NULL, "is_leaf: item location seems wrong (second one): %h\n", ih);
	    return 0;
	}
	prev_location = ih_location (ih);
    }

    // one may imagine much more checks
    return 1;
}


/* returns 1 if buf looks like an internal node, 0 otherwise */
static int is_internal (char * buf, int blocksize, struct buffer_head * bh)
{
    struct block_head * blkh;
    int nr;
    int used_space;

    blkh = (struct block_head *)buf;
    nr = blkh_level(blkh);
    if (nr <= DISK_LEAF_NODE_LEVEL || nr > MAX_HEIGHT) {
	/* this level is not possible for internal nodes */
	printk ("is_internal: this should be caught earlier\n");
	return 0;
    }
    
    nr = blkh_nr_item(blkh);
    if (nr > (blocksize - BLKH_SIZE - DC_SIZE) / (KEY_SIZE + DC_SIZE)) {
	/* for internal which is not root we might check min number of keys */
	reiserfs_warning (NULL, "is_internal: number of key seems wrong: %z\n", bh);
	return 0;
    }

    used_space = BLKH_SIZE + KEY_SIZE * nr + DC_SIZE * (nr + 1);
    if (used_space != blocksize - blkh_free_space(blkh)) {
	reiserfs_warning (NULL, "is_internal: free space seems wrong: %z\n", bh);
	return 0;
    }

    // one may imagine much more checks
    return 1;
}


// make sure that bh contains formatted node of reiserfs tree of
// 'level'-th level
static int is_tree_node (struct buffer_head * bh, int level)
{
    if (B_LEVEL (bh) != level) {
	printk ("is_tree_node: node level %d does not match to the expected one %d\n",
		B_LEVEL (bh), level);
	return 0;
    }
    if (level == DISK_LEAF_NODE_LEVEL)
	return is_leaf (bh->b_data, bh->b_size, bh);

    return is_internal (bh->b_data, bh->b_size, bh);
}



#ifdef SEARCH_BY_KEY_READA

/* The function is NOT SCHEDULE-SAFE! */
static void search_by_key_reada (struct super_block * s, int blocknr)
{
    struct buffer_head * bh;
  
    if (blocknr == 0)
	return;

    bh = getblk (s->s_dev, blocknr, s->s_blocksize);
  
    if (!buffer_uptodate (bh)) {
	ll_rw_block (READA, 1, &bh);
    }
    bh->b_count --;
}

#endif

/**************************************************************************
 * Algorithm   SearchByKey                                                *
 *             look for item in the Disk S+Tree by its key                *
 * Input:  p_s_sb   -  super block                                        *
 *         p_s_key  - pointer to the key to search                        *
 * Output: ITEM_FOUND, ITEM_NOT_FOUND or IO_ERROR                         *
 *         p_s_search_path - path from the root to the needed leaf        *
 **************************************************************************/

/* This function fills up the path from the root to the leaf as it
   descends the tree looking for the key.  It uses reiserfs_bread to
   try to find buffers in the cache given their block number.  If it
   does not find them in the cache it reads them from disk.  For each
   node search_by_key finds using reiserfs_bread it then uses
   bin_search to look through that node.  bin_search will find the
   position of the block_number of the next node if it is looking
   through an internal node.  If it is looking through a leaf node
   bin_search will find the position of the item which has key either
   equal to given key, or which is the maximal key less than the given
   key.  search_by_key returns a path that must be checked for the
   correctness of the top of the path but need not be checked for the
   correctness of the bottom of the path */
/* The function is NOT SCHEDULE-SAFE! */
int search_by_key (struct super_block * p_s_sb,
		   const struct cpu_key * p_s_key, /* Key to search. */
		   struct path * p_s_search_path, /* This structure was
						     allocated and initialized
						     by the calling
						     function. It is filled up
						     by this function.  */
		   int n_stop_level /* How far down the tree to search. To
                                       stop at leaf level - set to
                                       DISK_LEAF_NODE_LEVEL */
    ) {
    int  n_block_number = SB_ROOT_BLOCK (p_s_sb),
      expected_level = SB_TREE_HEIGHT (p_s_sb),
      n_block_size    = p_s_sb->s_blocksize;
    struct buffer_head  *       p_s_bh;
    struct path_element *       p_s_last_element;
    int				n_node_level, n_retval;
    int 			right_neighbor_of_leaf_node;
    int				fs_gen;

#ifdef CONFIG_REISERFS_CHECK
    int n_repeat_counter = 0;
#endif
    
    PROC_INFO_INC( p_s_sb, search_by_key );
    
    /* As we add each node to a path we increase its count.  This means that
       we must be careful to release all nodes in a path before we either
       discard the path struct or re-use the path struct, as we do here. */

    decrement_counters_in_path(p_s_search_path);

    right_neighbor_of_leaf_node = 0;

    /* With each iteration of this loop we search through the items in the
       current node, and calculate the next current node(next path element)
       for the next iteration of this loop.. */
    while ( 1 ) {

#ifdef CONFIG_REISERFS_CHECK
	if ( !(++n_repeat_counter % 50000) )
	    reiserfs_warning (p_s_sb, "PAP-5100: search_by_key: %s:"
			      "there were %d iterations of while loop "
			      "looking for key %K\n",
			      current->comm, n_repeat_counter, p_s_key);
#endif

	/* prep path to have another element added to it. */
	p_s_last_element = PATH_OFFSET_PELEMENT(p_s_search_path, ++p_s_search_path->path_length);
	fs_gen = get_generation (p_s_sb);
	expected_level --;

#ifdef SEARCH_BY_KEY_READA
	/* schedule read of right neighbor */
	search_by_key_reada (p_s_sb, right_neighbor_of_leaf_node);
#endif

	/* Read the next tree node, and set the last element in the path to
           have a pointer to it. */
	if ( ! (p_s_bh = p_s_last_element->pe_buffer =
		reiserfs_bread(p_s_sb, n_block_number, n_block_size)) ) {
	    p_s_search_path->path_length --;
	    pathrelse(p_s_search_path);
	    return IO_ERROR;
	}

 	if( fs_changed (fs_gen, p_s_sb) ) {
 		PROC_INFO_INC( p_s_sb, search_by_key_fs_changed );
 		PROC_INFO_INC( p_s_sb, sbk_fs_changed[ expected_level - 1 ] );
 	}

	/* It is possible that schedule occurred. We must check whether the key
	   to search is still in the tree rooted from the current buffer. If
	   not then repeat search from the root. */
	if ( fs_changed (fs_gen, p_s_sb) && 
	     (!B_IS_IN_TREE (p_s_bh) || !key_in_buffer(p_s_search_path, p_s_key, p_s_sb)) ) {
 	    PROC_INFO_INC( p_s_sb, search_by_key_restarted );
	    PROC_INFO_INC( p_s_sb, sbk_restarted[ expected_level - 1 ] );
	    decrement_counters_in_path(p_s_search_path);
	    
	    /* Get the root block number so that we can repeat the search
               starting from the root. */
	    n_block_number = SB_ROOT_BLOCK (p_s_sb);
	    expected_level = SB_TREE_HEIGHT (p_s_sb);
	    right_neighbor_of_leaf_node = 0;
	    
	    /* repeat search from the root */
	    continue;
	}

        /* only check that the key is in the buffer if p_s_key is not
           equal to the MAX_KEY. Latter case is only possible in
           "finish_unfinished()" processing during mount. */
        RFALSE( COMP_KEYS( &MAX_KEY, p_s_key ) && 
                ! key_in_buffer(p_s_search_path, p_s_key, p_s_sb),
		"PAP-5130: key is not in the buffer");
#ifdef CONFIG_REISERFS_CHECK
	if ( cur_tb ) {
	    print_cur_tb ("5140");
	    reiserfs_panic(p_s_sb, "PAP-5140: search_by_key: schedule occurred in do_balance!");
	}
#endif

	// make sure, that the node contents look like a node of
	// certain level
	if (!is_tree_node (p_s_bh, expected_level)) {
	    reiserfs_warning (p_s_sb, "vs-5150: search_by_key: "
			      "invalid format found in block %ld. Fsck?\n", 
			      p_s_bh->b_blocknr);
	    pathrelse (p_s_search_path);
	    return IO_ERROR;
	}
	
	/* ok, we have acquired next formatted node in the tree */
	n_node_level = B_LEVEL (p_s_bh);

	PROC_INFO_BH_STAT( p_s_sb, p_s_bh, n_node_level - 1 );

	RFALSE( n_node_level < n_stop_level,
		"vs-5152: tree level (%d) is less than stop level (%d)",
		n_node_level, n_stop_level);

	n_retval = bin_search( p_s_key, B_N_PITEM_HEAD(p_s_bh, 0),
                B_NR_ITEMS(p_s_bh),
                ( n_node_level == DISK_LEAF_NODE_LEVEL ) ? IH_SIZE : KEY_SIZE,
                &(p_s_last_element->pe_position));
	if (n_node_level == n_stop_level) {
	    return n_retval;
	}

	/* we are not in the stop level */
	if (n_retval == ITEM_FOUND)
	    /* item has been found, so we choose the pointer which is to the right of the found one */
	    p_s_last_element->pe_position++;

	/* if item was not found we choose the position which is to
	   the left of the found item. This requires no code,
	   bin_search did it already.*/

	/* So we have chosen a position in the current node which is
	   an internal node.  Now we calculate child block number by
	   position in the node. */
	n_block_number = B_N_CHILD_NUM(p_s_bh, p_s_last_element->pe_position);

#ifdef SEARCH_BY_KEY_READA
	/* if we are going to read leaf node, then calculate its right neighbor if possible */
	if (n_node_level == DISK_LEAF_NODE_LEVEL + 1 && p_s_last_element->pe_position < B_NR_ITEMS (p_s_bh))
	    right_neighbor_of_leaf_node = B_N_CHILD_NUM(p_s_bh, p_s_last_element->pe_position + 1);
#endif
    }
}


/* Form the path to an item and position in this item which contains
   file byte defined by p_s_key. If there is no such item
   corresponding to the key, we point the path to the item with
   maximal key less than p_s_key, and *p_n_pos_in_item is set to one
   past the last entry/byte in the item.  If searching for entry in a
   directory item, and it is not found, *p_n_pos_in_item is set to one
   entry more than the entry with maximal key which is less than the
   sought key.

   Note that if there is no entry in this same node which is one more,
   then we point to an imaginary entry.  for direct items, the
   position is in units of bytes, for indirect items the position is
   in units of blocknr entries, for directory items the position is in
   units of directory entries.  */

/* The function is NOT SCHEDULE-SAFE! */
int search_for_position_by_key (struct super_block  * p_s_sb,         /* Pointer to the super block.          */
				const struct cpu_key  * p_cpu_key,      /* Key to search (cpu variable)         */
				struct path         * p_s_search_path /* Filled up by this function.          */
    ) {
    struct item_head    * p_le_ih; /* pointer to on-disk structure */
    int                   n_blk_size;
    loff_t item_offset, offset;
    struct reiserfs_dir_entry de;
    int retval;

    /* If searching for directory entry. */
    if ( is_direntry_cpu_key (p_cpu_key) )
	return  search_by_entry_key (p_s_sb, p_cpu_key, p_s_search_path, &de);

    /* If not searching for directory entry. */
    
    /* If item is found. */
    retval = search_item (p_s_sb, p_cpu_key, p_s_search_path);
    if (retval == IO_ERROR)
	return retval;
    if ( retval == ITEM_FOUND )  {

	RFALSE( ! ih_item_len(
                B_N_PITEM_HEAD(PATH_PLAST_BUFFER(p_s_search_path),
			       PATH_LAST_POSITION(p_s_search_path))),
	        "PAP-5165: item length equals zero");

	pos_in_item(p_s_search_path) = 0;
	return POSITION_FOUND;
    }

    RFALSE( ! PATH_LAST_POSITION(p_s_search_path),
	    "PAP-5170: position equals zero");

    /* Item is not found. Set path to the previous item. */
    p_le_ih = B_N_PITEM_HEAD(PATH_PLAST_BUFFER(p_s_search_path), --PATH_LAST_POSITION(p_s_search_path));
    n_blk_size = p_s_sb->s_blocksize;

    if (comp_short_keys (&(p_le_ih->ih_key), p_cpu_key)) {
	return FILE_NOT_FOUND;
    }

    // FIXME: quite ugly this far

    item_offset = le_ih_k_offset (p_le_ih);
    offset = cpu_key_k_offset (p_cpu_key);

    /* Needed byte is contained in the item pointed to by the path.*/
    if (item_offset <= offset &&
	item_offset + op_bytes_number (p_le_ih, n_blk_size) > offset) {
	pos_in_item (p_s_search_path) = offset - item_offset;
	if ( is_indirect_le_ih(p_le_ih) ) {
	    pos_in_item (p_s_search_path) /= n_blk_size;
	}
	return POSITION_FOUND;
    }

    /* Needed byte is not contained in the item pointed to by the
     path. Set pos_in_item out of the item. */
    if ( is_indirect_le_ih (p_le_ih) )
	pos_in_item (p_s_search_path) = ih_item_len(p_le_ih) / UNFM_P_SIZE;
    else
        pos_in_item (p_s_search_path) = ih_item_len( p_le_ih );
  
    return POSITION_NOT_FOUND;
}


/* Compare given item and item pointed to by the path. */
int comp_items (const struct item_head * stored_ih, const struct path * p_s_path)
{
    struct buffer_head  * p_s_bh;
    struct item_head    * ih;

    /* Last buffer at the path is not in the tree. */
    if ( ! B_IS_IN_TREE(p_s_bh = PATH_PLAST_BUFFER(p_s_path)) )
	return 1;

    /* Last path position is invalid. */
    if ( PATH_LAST_POSITION(p_s_path) >= B_NR_ITEMS(p_s_bh) )
	return 1;

    /* we need only to know, whether it is the same item */
    ih = get_ih (p_s_path);
    return memcmp (stored_ih, ih, IH_SIZE);
}


/* unformatted nodes are not logged anymore, ever.  This is safe
** now
*/
#define held_by_others(bh) (atomic_read(&(bh)->b_count) > 1)

// block can not be forgotten as it is in I/O or held by someone
#define block_in_use(bh) (buffer_locked(bh) || (held_by_others(bh)))



// prepare for delete or cut of direct item
static inline int prepare_for_direct_item (struct path * path,
					   struct item_head * le_ih,
					   struct inode * inode,
					   loff_t new_file_length,
					   int * cut_size)
{
    loff_t round_len;


    if ( new_file_length == max_reiserfs_offset (inode) ) {
	/* item has to be deleted */
	*cut_size = -(IH_SIZE + ih_item_len(le_ih));
	return M_DELETE;
    }
	
    // new file gets truncated
    if (get_inode_item_key_version (inode) == KEY_FORMAT_3_6) {
	// 
	round_len = ROUND_UP (new_file_length); 
	/* this was n_new_file_length < le_ih ... */
	if ( round_len < le_ih_k_offset (le_ih) )  {
	    *cut_size = -(IH_SIZE + ih_item_len(le_ih));
	    return M_DELETE; /* Delete this item. */
	}
	/* Calculate first position and size for cutting from item. */
	pos_in_item (path) = round_len - (le_ih_k_offset (le_ih) - 1);
	*cut_size = -(ih_item_len(le_ih) - pos_in_item(path));
	
	return M_CUT; /* Cut from this item. */
    }


    // old file: items may have any length

    if ( new_file_length < le_ih_k_offset (le_ih) )  {
	*cut_size = -(IH_SIZE + ih_item_len(le_ih));
	return M_DELETE; /* Delete this item. */
    }
    /* Calculate first position and size for cutting from item. */
    *cut_size = -(ih_item_len(le_ih) -
		      (pos_in_item (path) = new_file_length + 1 - le_ih_k_offset (le_ih)));
    return M_CUT; /* Cut from this item. */
}


static inline int prepare_for_direntry_item (struct path * path,
					     struct item_head * le_ih,
					     struct inode * inode,
					     loff_t new_file_length,
					     int * cut_size)
{
    if (le_ih_k_offset (le_ih) == DOT_OFFSET && 
	new_file_length == max_reiserfs_offset (inode)) {
	RFALSE( ih_entry_count (le_ih) != 2,
	        "PAP-5220: incorrect empty directory item (%h)", le_ih);
	*cut_size = -(IH_SIZE + ih_item_len(le_ih));
	return M_DELETE; /* Delete the directory item containing "." and ".." entry. */
    }
    
    if ( ih_entry_count (le_ih) == 1 )  {
	/* Delete the directory item such as there is one record only
	   in this item*/
	*cut_size = -(IH_SIZE + ih_item_len(le_ih));
	return M_DELETE;
    }
    
    /* Cut one record from the directory item. */
    *cut_size = -(DEH_SIZE + entry_length (get_last_bh (path), le_ih, pos_in_item (path)));
    return M_CUT; 
}


/*  If the path points to a directory or direct item, calculate mode and the size cut, for balance.
    If the path points to an indirect item, remove some number of its unformatted nodes.
    In case of file truncate calculate whether this item must be deleted/truncated or last
    unformatted node of this item will be converted to a direct item.
    This function returns a determination of what balance mode the calling function should employ. */
static char  prepare_for_delete_or_cut(
				       struct reiserfs_transaction_handle *th, 
				       struct inode * inode,
				       struct path         * p_s_path,
				       const struct cpu_key      * p_s_item_key,
				       int                 * p_n_removed,      /* Number of unformatted nodes which were removed
										  from end of the file. */
				       int                 * p_n_cut_size,
				       unsigned long long    n_new_file_length /* MAX_KEY_OFFSET in case of delete. */
    ) {
    struct super_block  * p_s_sb = inode->i_sb;
    struct item_head    * p_le_ih = PATH_PITEM_HEAD(p_s_path);
    struct buffer_head  * p_s_bh = PATH_PLAST_BUFFER(p_s_path);

    /* Stat_data item. */
    if ( is_statdata_le_ih (p_le_ih) ) {

	RFALSE( n_new_file_length != max_reiserfs_offset (inode),
		"PAP-5210: mode must be M_DELETE");

	*p_n_cut_size = -(IH_SIZE + ih_item_len(p_le_ih));
	return M_DELETE;
    }


    /* Directory item. */
    if ( is_direntry_le_ih (p_le_ih) )
	return prepare_for_direntry_item (p_s_path, p_le_ih, inode, n_new_file_length, p_n_cut_size);

    /* Direct item. */
    if ( is_direct_le_ih (p_le_ih) )
	return prepare_for_direct_item (p_s_path, p_le_ih, inode, n_new_file_length, p_n_cut_size);


    /* Case of an indirect item. */
    {
	int                   n_unfm_number,    /* Number of the item unformatted nodes. */
	    n_counter,
	    n_blk_size;
	__u32               * p_n_unfm_pointer; /* Pointer to the unformatted node number. */
	__u32 tmp;
	struct item_head      s_ih;           /* Item header. */
	char                  c_mode;           /* Returned mode of the balance. */
	int need_research;


	n_blk_size = p_s_sb->s_blocksize;

	/* Search for the needed object indirect item until there are no unformatted nodes to be removed. */
	do  {
	    need_research = 0;
            p_s_bh = PATH_PLAST_BUFFER(p_s_path);
	    /* Copy indirect item header to a temp variable. */
	    copy_item_head(&s_ih, PATH_PITEM_HEAD(p_s_path));
	    /* Calculate number of unformatted nodes in this item. */
	    n_unfm_number = I_UNFM_NUM(&s_ih);

	    RFALSE( ! is_indirect_le_ih(&s_ih) || ! n_unfm_number ||
		    pos_in_item (p_s_path) + 1 !=  n_unfm_number,
		    "PAP-5240: illegal item %h "
		    "n_unfm_number = %d *p_n_pos_in_item = %d", 
		    &s_ih, n_unfm_number, pos_in_item (p_s_path));

	    /* Calculate balance mode and position in the item to remove unformatted nodes. */
	    if ( n_new_file_length == max_reiserfs_offset (inode) ) {/* Case of delete. */
		pos_in_item (p_s_path) = 0;
		*p_n_cut_size = -(IH_SIZE + ih_item_len(&s_ih));
		c_mode = M_DELETE;
	    }
	    else  { /* Case of truncate. */
		if ( n_new_file_length < le_ih_k_offset (&s_ih) )  {
		    pos_in_item (p_s_path) = 0;
		    *p_n_cut_size = -(IH_SIZE + ih_item_len(&s_ih));
		    c_mode = M_DELETE; /* Delete this item. */
		}
		else  {
		    /* indirect item must be truncated starting from *p_n_pos_in_item-th position */
		    pos_in_item (p_s_path) = (n_new_file_length + n_blk_size - le_ih_k_offset (&s_ih) ) >> p_s_sb->s_blocksize_bits;

		    RFALSE( pos_in_item (p_s_path) > n_unfm_number,
			    "PAP-5250: illegal position in the item");

		    /* Either convert last unformatted node of indirect item to direct item or increase
		       its free space.  */
		    if ( pos_in_item (p_s_path) == n_unfm_number )  {
			*p_n_cut_size = 0; /* Nothing to cut. */
			return M_CONVERT; /* Maybe convert last unformatted node to the direct item. */
		    }
		    /* Calculate size to cut. */
		    *p_n_cut_size = -(ih_item_len(&s_ih) - pos_in_item(p_s_path) * UNFM_P_SIZE);

		    c_mode = M_CUT;     /* Cut from this indirect item. */
		}
	    }

	    RFALSE( n_unfm_number <= pos_in_item (p_s_path),
		    "PAP-5260: illegal position in the indirect item");

	    /* pointers to be cut */
	    n_unfm_number -= pos_in_item (p_s_path);
	    /* Set pointer to the last unformatted node pointer that is to be cut. */
	    p_n_unfm_pointer = (__u32 *)B_I_PITEM(p_s_bh, &s_ih) + I_UNFM_NUM(&s_ih) - 1 - *p_n_removed;


	    /* We go through the unformatted nodes pointers of the indirect
	       item and look for the unformatted nodes in the cache. If we
	       found some of them we free it, zero corresponding indirect item
	       entry and log buffer containing that indirect item. For this we
	       need to prepare last path element for logging. If some
	       unformatted node has b_count > 1 we must not free this
	       unformatted node since it is in use. */
	    reiserfs_prepare_for_journal(p_s_sb, p_s_bh, 1);
	    // note: path could be changed, first line in for loop takes care
	    // of it

	    for (n_counter = *p_n_removed;
		 n_counter < n_unfm_number; n_counter++, p_n_unfm_pointer-- ) {

		if (item_moved (&s_ih, p_s_path)) {
		    need_research = 1 ;
		    break;
		}
		RFALSE( p_n_unfm_pointer < (__u32 *)B_I_PITEM(p_s_bh, &s_ih) ||
			p_n_unfm_pointer > (__u32 *)B_I_PITEM(p_s_bh, &s_ih) + I_UNFM_NUM(&s_ih) - 1,
			"vs-5265: pointer out of range");

		/* Hole, nothing to remove. */
		if ( ! get_block_num(p_n_unfm_pointer,0) )  { 
			(*p_n_removed)++;
			continue;
		}

		(*p_n_removed)++;

		tmp = get_block_num(p_n_unfm_pointer,0);
		put_block_num(p_n_unfm_pointer, 0, 0);
		journal_mark_dirty (th, p_s_sb, p_s_bh);
		inode->i_blocks -= p_s_sb->s_blocksize / 512;
		reiserfs_free_block(th, tmp);
		/* In case of big fragmentation it is possible that each block
		   freed will cause dirtying of one more bitmap and then we will
		   quickly overflow our transaction space. This is a
		   counter-measure against that scenario */
		if (journal_transaction_should_end(th, th->t_blocks_allocated)) {
		    int orig_len_alloc = th->t_blocks_allocated ;
		    pathrelse(p_s_path) ;

		    journal_end(th, p_s_sb, orig_len_alloc) ;
		    journal_begin(th, p_s_sb, orig_len_alloc) ;
		    reiserfs_update_inode_transaction(inode) ;
		    need_research = 1;
		    break;
		}

		if ( item_moved (&s_ih, p_s_path) )  {
			need_research = 1;
			break ;
		}
	    }

	    /* a trick.  If the buffer has been logged, this
	    ** will do nothing.  If we've broken the loop without
	    ** logging it, it will restore the buffer
	    **
	    */
	    reiserfs_restore_prepared_buffer(p_s_sb, p_s_bh);

	    /* This loop can be optimized. */
	} while ( (*p_n_removed < n_unfm_number || need_research) &&
		  search_for_position_by_key(p_s_sb, p_s_item_key, p_s_path) == POSITION_FOUND );

	RFALSE( *p_n_removed < n_unfm_number, 
		"PAP-5310: indirect item is not found");
	RFALSE( item_moved (&s_ih, p_s_path), 
		"after while, comp failed, retry") ;

	if (c_mode == M_CUT)
	    pos_in_item (p_s_path) *= UNFM_P_SIZE;
	return c_mode;
    }
}


/* Calculate bytes number which will be deleted or cutted in the balance. */
int calc_deleted_bytes_number(
    struct  tree_balance  * p_s_tb,
    char                    c_mode
    ) {
    int                     n_del_size;
    struct  item_head     * p_le_ih = PATH_PITEM_HEAD(p_s_tb->tb_path);

    if ( is_statdata_le_ih (p_le_ih) )
	return 0;

    if ( is_direntry_le_ih (p_le_ih) ) {
	// return EMPTY_DIR_SIZE; /* We delete emty directoris only. */
	// we can't use EMPTY_DIR_SIZE, as old format dirs have a different
	// empty size.  ick. FIXME, is this right?
	//
	return ih_item_len(p_le_ih);
    }
    n_del_size = ( c_mode == M_DELETE ) ? ih_item_len(p_le_ih) : -p_s_tb->insert_size[0];

    if ( is_indirect_le_ih (p_le_ih) )
	n_del_size = (n_del_size/UNFM_P_SIZE)*
	  (PATH_PLAST_BUFFER(p_s_tb->tb_path)->b_size);// - get_ih_free_space (p_le_ih);
    return n_del_size;
}

static void init_tb_struct(
    struct reiserfs_transaction_handle *th,
    struct tree_balance * p_s_tb,
    struct super_block  * p_s_sb,
    struct path         * p_s_path,
    int                   n_size
    ) {
    memset (p_s_tb,'\0',sizeof(struct tree_balance));
    p_s_tb->transaction_handle = th ;
    p_s_tb->tb_sb = p_s_sb;
    p_s_tb->tb_path = p_s_path;
    PATH_OFFSET_PBUFFER(p_s_path, ILLEGAL_PATH_ELEMENT_OFFSET) = NULL;
    PATH_OFFSET_POSITION(p_s_path, ILLEGAL_PATH_ELEMENT_OFFSET) = 0;
    p_s_tb->insert_size[0] = n_size;
}



void padd_item (char * item, int total_length, int length)
{
    int i;

    for (i = total_length; i > length; )
	item [--i] = 0;
}


/* Delete object item. */
int reiserfs_delete_item (struct reiserfs_transaction_handle *th, 
			  struct path * p_s_path, /* Path to the deleted item. */
			  const struct cpu_key * p_s_item_key, /* Key to search for the deleted item.  */
			  struct inode * p_s_inode,/* inode is here just to update i_blocks */
			  struct buffer_head  * p_s_un_bh)    /* NULL or unformatted node pointer.    */
{
    struct super_block * p_s_sb = p_s_inode->i_sb;
    struct tree_balance   s_del_balance;
    struct item_head      s_ih;
    int                   n_ret_value,
	n_del_size,
	n_removed;

#ifdef CONFIG_REISERFS_CHECK
    char                  c_mode;
    int			n_iter = 0;
#endif

    init_tb_struct(th, &s_del_balance, p_s_sb, p_s_path, 0/*size is unknown*/);

    while ( 1 ) {
	n_removed = 0;

#ifdef CONFIG_REISERFS_CHECK
	n_iter++;
	c_mode =
#endif
	    prepare_for_delete_or_cut(th, p_s_inode, p_s_path, p_s_item_key, &n_removed, &n_del_size, max_reiserfs_offset (p_s_inode));

	RFALSE( c_mode != M_DELETE, "PAP-5320: mode must be M_DELETE");

	copy_item_head(&s_ih, PATH_PITEM_HEAD(p_s_path));
	s_del_balance.insert_size[0] = n_del_size;

	n_ret_value = fix_nodes(M_DELETE, &s_del_balance, NULL, 0);
	if ( n_ret_value != REPEAT_SEARCH )
	    break;

	PROC_INFO_INC( p_s_sb, delete_item_restarted );

	// file system changed, repeat search
	n_ret_value = search_for_position_by_key(p_s_sb, p_s_item_key, p_s_path);
	if (n_ret_value == IO_ERROR)
	    break;
	if (n_ret_value == FILE_NOT_FOUND) {
	    reiserfs_warning (p_s_sb, "vs-5340: reiserfs_delete_item: "
			      "no items of the file %K found\n", p_s_item_key);
	    break;
	}
    } /* while (1) */

    if ( n_ret_value != CARRY_ON ) {
	unfix_nodes(&s_del_balance);
	return 0;
    }

    // reiserfs_delete_item returns item length when success
    n_ret_value = calc_deleted_bytes_number(&s_del_balance, M_DELETE);

    if ( p_s_un_bh )  {
	int off;
        char *data ;

	/* We are in direct2indirect conversion, so move tail contents
           to the unformatted node */
	/* note, we do the copy before preparing the buffer because we
	** don't care about the contents of the unformatted node yet.
	** the only thing we really care about is the direct item's data
	** is in the unformatted node.
	**
	** Otherwise, we would have to call reiserfs_prepare_for_journal on
	** the unformatted node, which might schedule, meaning we'd have to
	** loop all the way back up to the start of the while loop.
	**
	** The unformatted node must be dirtied later on.  We can't be
	** sure here if the entire tail has been deleted yet.
        **
        ** p_s_un_bh is from the page cache (all unformatted nodes are
        ** from the page cache) and might be a highmem page.  So, we
        ** can't use p_s_un_bh->b_data.  But, the page has already been
        ** kmapped, so we can use page_address()
	** -clm
	*/

        data = page_address(p_s_un_bh->b_page) ;
	off = ((le_ih_k_offset (&s_ih) - 1) & (PAGE_CACHE_SIZE - 1));
	memcpy(data + off,
	       B_I_PITEM(PATH_PLAST_BUFFER(p_s_path), &s_ih), n_ret_value);
    }

    /* Perform balancing after all resources have been collected at once. */ 
    do_balance(&s_del_balance, NULL, NULL, M_DELETE);

    /* Return deleted body length */
    return n_ret_value;
}


/* Summary Of Mechanisms For Handling Collisions Between Processes:

 deletion of the body of the object is performed by iput(), with the
 result that if multiple processes are operating on a file, the
 deletion of the body of the file is deferred until the last process
 that has an open inode performs its iput().

 writes and truncates are protected from collisions by use of
 semaphores.

 creates, linking, and mknod are protected from collisions with other
 processes by making the reiserfs_add_entry() the last step in the
 creation, and then rolling back all changes if there was a collision.
 - Hans
*/


/* this deletes item which never gets split */
void reiserfs_delete_solid_item (struct reiserfs_transaction_handle *th,
				 struct key * key)
{
    struct tree_balance tb;
    INITIALIZE_PATH (path);
    int item_len;
    int tb_init = 0 ;
    struct cpu_key cpu_key;
    int retval;
    
    le_key2cpu_key (&cpu_key, key);
    
    while (1) {
	retval = search_item (th->t_super, &cpu_key, &path);
	if (retval == IO_ERROR) {
	    reiserfs_warning (th->t_super, "vs-5350: reiserfs_delete_solid_item: "
			      "i/o failure occurred trying to delete %K\n", &cpu_key);
	    break;
	}
	if (retval != ITEM_FOUND) {
	    pathrelse (&path);
	    // No need for a warning, if there is just no free space to insert '..' item into the newly-created subdir
	    if ( !( (unsigned long long) GET_HASH_VALUE (le_key_k_offset (le_key_version (key), key)) == 0 && \
		 GET_GENERATION_NUMBER (le_key_k_offset (le_key_version (key), key)) == 1 ) )
		reiserfs_warning (th->t_super, "vs-5355: reiserfs_delete_solid_item: %k not found\n", key);
	    break;
	}
	if (!tb_init) {
	    tb_init = 1 ;
	    item_len = ih_item_len( PATH_PITEM_HEAD(&path) );
	    init_tb_struct (th, &tb, th->t_super, &path, - (IH_SIZE + item_len));
	}

	retval = fix_nodes (M_DELETE, &tb, NULL, 0);
	if (retval == REPEAT_SEARCH) {
	    PROC_INFO_INC( th -> t_super, delete_solid_item_restarted );
	    continue;
	}

	if (retval == CARRY_ON) {
	    do_balance (&tb, 0, 0, M_DELETE);
	    break;
	}

	// IO_ERROR, NO_DISK_SPACE, etc
	reiserfs_warning (th->t_super, "vs-5360: reiserfs_delete_solid_item: "
			  "could not delete %K due to fix_nodes failure\n", &cpu_key);
	unfix_nodes (&tb);
	break;
    }

    reiserfs_check_path(&path) ;
}


void reiserfs_delete_object (struct reiserfs_transaction_handle *th, struct inode * inode)
{
    inode->i_size = 0;

    /* for directory this deletes item containing "." and ".." */
    reiserfs_do_truncate (th, inode, NULL, 0/*no timestamp updates*/);
    
#if defined( USE_INODE_GENERATION_COUNTER )
    if( !old_format_only ( th -> t_super ) )
      {
       __u32 *inode_generation;
       
       inode_generation = 
         &th -> t_super -> u.reiserfs_sb.s_rs -> s_inode_generation;
       *inode_generation = cpu_to_le32( le32_to_cpu( *inode_generation ) + 1 );
      }
/* USE_INODE_GENERATION_COUNTER */
#endif
    reiserfs_delete_solid_item (th, INODE_PKEY (inode));
}


static int maybe_indirect_to_direct (struct reiserfs_transaction_handle *th, 
			      struct inode * p_s_inode,
			      struct page *page, 
			      struct path         * p_s_path,
			      const struct cpu_key      * p_s_item_key,
			      loff_t         n_new_file_size,
			      char                * p_c_mode
			      ) {
    struct super_block * p_s_sb = p_s_inode->i_sb;
    int n_block_size = p_s_sb->s_blocksize;
    int cut_bytes;

    if (n_new_file_size != p_s_inode->i_size)
	BUG ();

    /* the page being sent in could be NULL if there was an i/o error
    ** reading in the last block.  The user will hit problems trying to
    ** read the file, but for now we just skip the indirect2direct
    */
    if (atomic_read(&p_s_inode->i_count) > 1 || 
        !tail_has_to_be_packed (p_s_inode) || 
        !page || (p_s_inode->u.reiserfs_i.i_flags & i_nopack_mask)) {
	// leave tail in an unformatted node	
	*p_c_mode = M_SKIP_BALANCING;
	cut_bytes = n_block_size - (n_new_file_size & (n_block_size - 1));
	pathrelse(p_s_path);
	return cut_bytes;
    }
    /* Permorm the conversion to a direct_item. */
    /*return indirect_to_direct (p_s_inode, p_s_path, p_s_item_key, n_new_file_size, p_c_mode);*/
    return indirect2direct (th, p_s_inode, page, p_s_path, p_s_item_key, n_new_file_size, p_c_mode);
}


/* we did indirect_to_direct conversion. And we have inserted direct
   item successesfully, but there were no disk space to cut unfm
   pointer being converted. Therefore we have to delete inserted
   direct item(s) */
static void indirect_to_direct_roll_back (struct reiserfs_transaction_handle *th, struct inode * inode, struct path * path)
{
    struct cpu_key tail_key;
    int tail_len;
    int removed;

    make_cpu_key (&tail_key, inode, inode->i_size + 1, TYPE_DIRECT, 4);// !!!!
    tail_key.key_length = 4;

    tail_len = (cpu_key_k_offset (&tail_key) & (inode->i_sb->s_blocksize - 1)) - 1;
    while (tail_len) {
	/* look for the last byte of the tail */
	if (search_for_position_by_key (inode->i_sb, &tail_key, path) == POSITION_NOT_FOUND)
	    reiserfs_panic (inode->i_sb, "vs-5615: indirect_to_direct_roll_back: found invalid item");
	RFALSE( path->pos_in_item != ih_item_len(PATH_PITEM_HEAD (path)) - 1,
	        "vs-5616: appended bytes found");
	PATH_LAST_POSITION (path) --;
	
	removed = reiserfs_delete_item (th, path, &tail_key, inode, 0/*unbh not needed*/);
	RFALSE( removed <= 0 || removed > tail_len,
	        "vs-5617: there was tail %d bytes, removed item length %d bytes",
                tail_len, removed);
	tail_len -= removed;
	set_cpu_key_k_offset (&tail_key, cpu_key_k_offset (&tail_key) - removed);
    }
    reiserfs_warning (inode->i_sb, "indirect_to_direct_roll_back: indirect_to_direct conversion has been rolled back due to lack of disk space\n");
    //mark_file_without_tail (inode);
    mark_inode_dirty (inode);
}


/* (Truncate or cut entry) or delete object item. Returns < 0 on failure */
int reiserfs_cut_from_item (struct reiserfs_transaction_handle *th, 
			    struct path * p_s_path,
			    struct cpu_key * p_s_item_key,
			    struct inode * p_s_inode,
			    struct page *page, 
			    loff_t n_new_file_size)
{
    struct super_block * p_s_sb = p_s_inode->i_sb;
    /* Every function which is going to call do_balance must first
       create a tree_balance structure.  Then it must fill up this
       structure by using the init_tb_struct and fix_nodes functions.
       After that we can make tree balancing. */
    struct tree_balance s_cut_balance;
    int n_cut_size = 0,        /* Amount to be cut. */
	n_ret_value = CARRY_ON,
	n_removed = 0,     /* Number of the removed unformatted nodes. */
	n_is_inode_locked = 0;
    char                c_mode;            /* Mode of the balance. */
    int retval2 = -1;
    
    
    init_tb_struct(th, &s_cut_balance, p_s_inode->i_sb, p_s_path, n_cut_size);


    /* Repeat this loop until we either cut the item without needing
       to balance, or we fix_nodes without schedule occuring */
    while ( 1 ) {
	/* Determine the balance mode, position of the first byte to
	   be cut, and size to be cut.  In case of the indirect item
	   free unformatted nodes which are pointed to by the cut
	   pointers. */
      
	c_mode = prepare_for_delete_or_cut(th, p_s_inode, p_s_path, p_s_item_key, &n_removed, 
					   &n_cut_size, n_new_file_size);
	if ( c_mode == M_CONVERT )  {
	    /* convert last unformatted node to direct item or leave
               tail in the unformatted node */
	    RFALSE( n_ret_value != CARRY_ON, "PAP-5570: can not convert twice");

	    n_ret_value = maybe_indirect_to_direct (th, p_s_inode, page, p_s_path, p_s_item_key,
						    n_new_file_size, &c_mode);
	    if ( c_mode == M_SKIP_BALANCING )
		/* tail has been left in the unformatted node */
		return n_ret_value;

	    n_is_inode_locked = 1;
	  
	    /* removing of last unformatted node will change value we
               have to return to truncate. Save it */
	    retval2 = n_ret_value;
	    /*retval2 = p_s_sb->s_blocksize - (n_new_file_size & (p_s_sb->s_blocksize - 1));*/
	  
	    /* So, we have performed the first part of the conversion:
	       inserting the new direct item.  Now we are removing the
	       last unformatted node pointer. Set key to search for
	       it. */
      	    set_cpu_key_k_type (p_s_item_key, TYPE_INDIRECT);
	    p_s_item_key->key_length = 4;
	    n_new_file_size -= (n_new_file_size & (p_s_sb->s_blocksize - 1));
	    set_cpu_key_k_offset (p_s_item_key, n_new_file_size + 1);
	    if ( search_for_position_by_key(p_s_sb, p_s_item_key, p_s_path) == POSITION_NOT_FOUND ){
		print_block (PATH_PLAST_BUFFER (p_s_path), 3, PATH_LAST_POSITION (p_s_path) - 1, PATH_LAST_POSITION (p_s_path) + 1);
		reiserfs_panic(p_s_sb, "PAP-5580: reiserfs_cut_from_item: item to convert does not exist (%K)", p_s_item_key);
	    }
	    continue;
	}
	if (n_cut_size == 0) {
	    pathrelse (p_s_path);
	    return 0;
	}

	s_cut_balance.insert_size[0] = n_cut_size;
	
	n_ret_value = fix_nodes(c_mode, &s_cut_balance, NULL, 0);
      	if ( n_ret_value != REPEAT_SEARCH )
	    break;
	
	PROC_INFO_INC( p_s_sb, cut_from_item_restarted );

	n_ret_value = search_for_position_by_key(p_s_sb, p_s_item_key, p_s_path);
	if (n_ret_value == POSITION_FOUND)
	    continue;

	reiserfs_warning (p_s_sb, "PAP-5610: reiserfs_cut_from_item: item %K not found\n", p_s_item_key);
	unfix_nodes (&s_cut_balance);
	return (n_ret_value == IO_ERROR) ? -EIO : -ENOENT;
    } /* while */
  
    // check fix_nodes results (IO_ERROR or NO_DISK_SPACE)
    if ( n_ret_value != CARRY_ON ) {
	if ( n_is_inode_locked ) {
	    // FIXME: this seems to be not needed: we are always able
	    // to cut item
	    indirect_to_direct_roll_back (th, p_s_inode, p_s_path);
	}
	if (n_ret_value == NO_DISK_SPACE)
	    reiserfs_warning (p_s_sb, "NO_DISK_SPACE\n");
	unfix_nodes (&s_cut_balance);
	return -EIO;
    }

    /* go ahead and perform balancing */
    
    RFALSE( c_mode == M_PASTE || c_mode == M_INSERT, "illegal mode");

    /* Calculate number of bytes that need to be cut from the item. */
    if (retval2 == -1)
	n_ret_value = calc_deleted_bytes_number(&s_cut_balance, c_mode);
    else
	n_ret_value = retval2;
    
    if ( c_mode == M_DELETE ) {
	struct item_head * p_le_ih = PATH_PITEM_HEAD (s_cut_balance.tb_path);
	
	if ( is_direct_le_ih (p_le_ih) && (le_ih_k_offset (p_le_ih) & (p_s_sb->s_blocksize - 1)) == 1 ) {
	    /* we delete first part of tail which was stored in direct
               item(s) */
	    // FIXME: this is to keep 3.5 happy
	    p_s_inode->u.reiserfs_i.i_first_direct_byte = U32_MAX;
	    p_s_inode->i_blocks -= p_s_sb->s_blocksize / 512;
	}
    }

#ifdef CONFIG_REISERFS_CHECK
    if (n_is_inode_locked) {
	struct item_head * le_ih = PATH_PITEM_HEAD (s_cut_balance.tb_path);
	/* we are going to complete indirect2direct conversion. Make
           sure, that we exactly remove last unformatted node pointer
           of the item */
	if (!is_indirect_le_ih (le_ih))
	    reiserfs_panic (p_s_sb, "vs-5652: reiserfs_cut_from_item: "
			    "item must be indirect %h", le_ih);

	if (c_mode == M_DELETE && ih_item_len(le_ih) != UNFM_P_SIZE)
	    reiserfs_panic (p_s_sb, "vs-5653: reiserfs_cut_from_item: "
			    "completing indirect2direct conversion indirect item %h "
			    "being deleted must be of 4 byte long", le_ih);

	if (c_mode == M_CUT && s_cut_balance.insert_size[0] != -UNFM_P_SIZE) {
	    reiserfs_panic (p_s_sb, "vs-5654: reiserfs_cut_from_item: "
			    "can not complete indirect2direct conversion of %h (CUT, insert_size==%d)",
			    le_ih, s_cut_balance.insert_size[0]);
	}
	/* it would be useful to make sure, that right neighboring
           item is direct item of this file */
    }
#endif
    
    do_balance(&s_cut_balance, NULL, NULL, c_mode);
    if ( n_is_inode_locked ) {
	/* we've done an indirect->direct conversion.  when the data block
	** was freed, it was removed from the list of blocks that must
	** be flushed before the transaction commits, so we don't need to
	** deal with it here.
	*/
	p_s_inode->u.reiserfs_i.i_flags &= ~i_pack_on_close_mask;
    }
    return n_ret_value;
}


static void truncate_directory (struct reiserfs_transaction_handle *th, struct inode * inode)
{
    if (inode->i_nlink)
	reiserfs_warning (th->t_super, "vs-5655: truncate_directory: link count != 0\n");

    set_le_key_k_offset (KEY_FORMAT_3_5, INODE_PKEY (inode), DOT_OFFSET);
    set_le_key_k_type (KEY_FORMAT_3_5, INODE_PKEY (inode), TYPE_DIRENTRY);
    reiserfs_delete_solid_item (th, INODE_PKEY (inode));

    set_le_key_k_offset (KEY_FORMAT_3_5, INODE_PKEY (inode), SD_OFFSET);
    set_le_key_k_type (KEY_FORMAT_3_5, INODE_PKEY (inode), TYPE_STAT_DATA);    
}




/* Truncate file to the new size. Note, this must be called with a transaction
   already started */
void reiserfs_do_truncate (struct reiserfs_transaction_handle *th, 
			   struct  inode * p_s_inode, /* ->i_size contains new
                                                         size */
			   struct page *page, /* up to date for last block */
			   int update_timestamps  /* when it is called by
						     file_release to convert
						     the tail - no timestamps
						     should be updated */
    ) {
    INITIALIZE_PATH (s_search_path);       /* Path to the current object item. */
    struct item_head    * p_le_ih;         /* Pointer to an item header. */
    struct cpu_key      s_item_key;     /* Key to search for a previous file item. */
    loff_t         n_file_size,    /* Old file size. */
	n_new_file_size;/* New file size. */
    int                   n_deleted;      /* Number of deleted or truncated bytes. */
    int retval;

    if ( ! (S_ISREG(p_s_inode->i_mode) || S_ISDIR(p_s_inode->i_mode) || S_ISLNK(p_s_inode->i_mode)) )
	return;

    if (S_ISDIR(p_s_inode->i_mode)) {
	// deletion of directory - no need to update timestamps
	truncate_directory (th, p_s_inode);
	return;
    }

    /* Get new file size. */
    n_new_file_size = p_s_inode->i_size;

    // FIXME: note, that key type is unimportant here
    make_cpu_key (&s_item_key, p_s_inode, max_reiserfs_offset (p_s_inode), TYPE_DIRECT, 3);

    retval = search_for_position_by_key(p_s_inode->i_sb, &s_item_key, &s_search_path);
    if (retval == IO_ERROR) {
	reiserfs_warning (p_s_inode->i_sb, "vs-5657: reiserfs_do_truncate: "
			  "i/o failure occurred trying to truncate %K\n", &s_item_key);
	return;
    }
    if (retval == POSITION_FOUND || retval == FILE_NOT_FOUND) {
	pathrelse (&s_search_path);
	reiserfs_warning (p_s_inode->i_sb, "PAP-5660: reiserfs_do_truncate: "
			  "wrong result %d of search for %K\n", retval, &s_item_key);
	return;
    }

    s_search_path.pos_in_item --;

    /* Get real file size (total length of all file items) */
    p_le_ih = PATH_PITEM_HEAD(&s_search_path);
    if ( is_statdata_le_ih (p_le_ih) )
	n_file_size = 0;
    else {
	loff_t offset = le_ih_k_offset (p_le_ih);
	int bytes = op_bytes_number (p_le_ih,p_s_inode->i_sb->s_blocksize);

	/* this may mismatch with real file size: if last direct item
           had no padding zeros and last unformatted node had no free
           space, this file would have this file size */
	n_file_size = offset + bytes - 1;
    }

    if ( n_file_size == 0 || n_file_size < n_new_file_size ) {
	goto update_and_out ;
    }

    /* Update key to search for the last file item. */
    set_cpu_key_k_offset (&s_item_key, n_file_size);

    do  {
	/* Cut or delete file item. */
	n_deleted = reiserfs_cut_from_item(th, &s_search_path, &s_item_key, p_s_inode,  page, n_new_file_size);
	if (n_deleted < 0) {
	    reiserfs_warning (th->t_super, "vs-5665: reiserfs_truncate_file: cut_from_item failed\n");
	    reiserfs_check_path(&s_search_path) ;
	    return;
	}

	RFALSE( n_deleted > n_file_size,
		"PAP-5670: reiserfs_truncate_file returns too big number: deleted %d, file_size %lu, item_key %K",
		n_deleted, n_file_size, &s_item_key);

	/* Change key to search the last file item. */
	n_file_size -= n_deleted;

	set_cpu_key_k_offset (&s_item_key, n_file_size);

	/* While there are bytes to truncate and previous file item is presented in the tree. */

	/*
	** This loop could take a really long time, and could log 
	** many more blocks than a transaction can hold.  So, we do a polite
	** journal end here, and if the transaction needs ending, we make
	** sure the file is consistent before ending the current trans
	** and starting a new one
	*/
        if (journal_transaction_should_end(th, th->t_blocks_allocated)) {
	  int orig_len_alloc = th->t_blocks_allocated ;
	  decrement_counters_in_path(&s_search_path) ;

	  if (update_timestamps) {
	      p_s_inode->i_mtime = p_s_inode->i_ctime = CURRENT_TIME;
	  } 
	  reiserfs_update_sd(th, p_s_inode) ;

	  journal_end(th, p_s_inode->i_sb, orig_len_alloc) ;
	  journal_begin(th, p_s_inode->i_sb, orig_len_alloc) ;
	  reiserfs_update_inode_transaction(p_s_inode) ;
	}
    } while ( n_file_size > ROUND_UP (n_new_file_size) &&
	      search_for_position_by_key(p_s_inode->i_sb, &s_item_key, &s_search_path) == POSITION_FOUND )  ;

    RFALSE( n_file_size > ROUND_UP (n_new_file_size),
	    "PAP-5680: truncate did not finish: new_file_size %Ld, current %Ld, oid %d\n",
	    n_new_file_size, n_file_size, s_item_key.on_disk_key.k_objectid);

update_and_out:
    if (update_timestamps) {
	// this is truncate, not file closing
	p_s_inode->i_mtime = p_s_inode->i_ctime = CURRENT_TIME;
    }
    reiserfs_update_sd (th, p_s_inode);

    pathrelse(&s_search_path) ;
}


#ifdef CONFIG_REISERFS_CHECK
// this makes sure, that we __append__, not overwrite or add holes
static void check_research_for_paste (struct path * path, 
				      const struct cpu_key * p_s_key)
{
    struct item_head * found_ih = get_ih (path);
    
    if (is_direct_le_ih (found_ih)) {
	if (le_ih_k_offset (found_ih) + op_bytes_number (found_ih, get_last_bh (path)->b_size) !=
	    cpu_key_k_offset (p_s_key) ||
	    op_bytes_number (found_ih, get_last_bh (path)->b_size) != pos_in_item (path))
	    reiserfs_panic (0, "PAP-5720: check_research_for_paste: "
			    "found direct item %h or position (%d) does not match to key %K",
			    found_ih, pos_in_item (path), p_s_key);
    }
    if (is_indirect_le_ih (found_ih)) {
	if (le_ih_k_offset (found_ih) + op_bytes_number (found_ih, get_last_bh (path)->b_size) != cpu_key_k_offset (p_s_key) || 
	    I_UNFM_NUM (found_ih) != pos_in_item (path) ||
	    get_ih_free_space (found_ih) != 0)
	    reiserfs_panic (0, "PAP-5730: check_research_for_paste: "
			    "found indirect item (%h) or position (%d) does not match to key (%K)",
			    found_ih, pos_in_item (path), p_s_key);
    }
}
#endif /* config reiserfs check */


/* Paste bytes to the existing item. Returns bytes number pasted into the item. */
int reiserfs_paste_into_item (struct reiserfs_transaction_handle *th, 
			      struct path         * p_s_search_path,	/* Path to the pasted item.          */
			      const struct cpu_key      * p_s_key,        	/* Key to search for the needed item.*/
			      const char          * p_c_body,       	/* Pointer to the bytes to paste.    */
			      int                   n_pasted_size)  	/* Size of pasted bytes.             */
{
    struct tree_balance s_paste_balance;
    int                 retval;

    init_tb_struct(th, &s_paste_balance, th->t_super, p_s_search_path, n_pasted_size);
#ifdef DISPLACE_NEW_PACKING_LOCALITIES
    s_paste_balance.key = p_s_key->on_disk_key;
#endif
    
    while ( (retval = fix_nodes(M_PASTE, &s_paste_balance, NULL, p_c_body)) == REPEAT_SEARCH ) {
	/* file system changed while we were in the fix_nodes */
	PROC_INFO_INC( th -> t_super, paste_into_item_restarted );
	retval = search_for_position_by_key (th->t_super, p_s_key, p_s_search_path);
	if (retval == IO_ERROR) {
	    retval = -EIO ;
	    goto error_out ;
	}
	if (retval == POSITION_FOUND) {
	    reiserfs_warning (th->t_super, "PAP-5710: reiserfs_paste_into_item: entry or pasted byte (%K) exists\n", p_s_key);
	    retval = -EEXIST ;
	    goto error_out ;
	}
	
#ifdef CONFIG_REISERFS_CHECK
	check_research_for_paste (p_s_search_path, p_s_key);
#endif
    }

    /* Perform balancing after all resources are collected by fix_nodes, and
       accessing them will not risk triggering schedule. */
    if ( retval == CARRY_ON ) {
	do_balance(&s_paste_balance, NULL/*ih*/, p_c_body, M_PASTE);
	return 0;
    }
    retval = (retval == NO_DISK_SPACE) ? -ENOSPC : -EIO;
error_out:
    /* this also releases the path */
    unfix_nodes(&s_paste_balance);
    return retval ;
}


/* Insert new item into the buffer at the path. */
int reiserfs_insert_item(struct reiserfs_transaction_handle *th, 
			 struct path         * 	p_s_path,         /* Path to the inserteded item.         */
			 const struct cpu_key      * key,
			 struct item_head    * 	p_s_ih,           /* Pointer to the item header to insert.*/
			 const char          * 	p_c_body)         /* Pointer to the bytes to insert.      */
{
    struct tree_balance s_ins_balance;
    int                 retval;

    init_tb_struct(th, &s_ins_balance, th->t_super, p_s_path, IH_SIZE + ih_item_len(p_s_ih));
#ifdef DISPLACE_NEW_PACKING_LOCALITIES
    s_ins_balance.key = key->on_disk_key;
#endif

    /*
    if (p_c_body == 0)
      n_zeros_num = ih_item_len(p_s_ih);
    */
    //    le_key2cpu_key (&key, &(p_s_ih->ih_key));

    while ( (retval = fix_nodes(M_INSERT, &s_ins_balance, p_s_ih, p_c_body)) == REPEAT_SEARCH) {
	/* file system changed while we were in the fix_nodes */
	PROC_INFO_INC( th -> t_super, insert_item_restarted );
	retval = search_item (th->t_super, key, p_s_path);
	if (retval == IO_ERROR) {
	    retval = -EIO;
	    goto error_out ;
	}
	if (retval == ITEM_FOUND) {
	    reiserfs_warning (th->t_super, "PAP-5760: reiserfs_insert_item: "
			      "key %K already exists in the tree\n", key);
	    retval = -EEXIST ;
	    goto error_out; 
	}
    }

    /* make balancing after all resources will be collected at a time */ 
    if ( retval == CARRY_ON ) {
	do_balance (&s_ins_balance, p_s_ih, p_c_body, M_INSERT);
	return 0;
    }

    retval = (retval == NO_DISK_SPACE) ? -ENOSPC : -EIO;
error_out:
    /* also releases the path */
    unfix_nodes(&s_ins_balance);
    return retval; 
}




