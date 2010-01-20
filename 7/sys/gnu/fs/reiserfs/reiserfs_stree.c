/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

/* Minimal possible key. It is never in the tree. */
const struct key MIN_KEY = {
	0,
	0,
	{ {0, 0}, }
};

/* Maximal possible key. It is never in the tree. */
const struct key MAX_KEY = {
	0xffffffff,
	0xffffffff,
	{ {0xffffffff, 0xffffffff }, }
};

/* Does the buffer contain a disk block which is in the tree. */
int
B_IS_IN_TREE(const struct buf *p_s_bp)
{

	return (B_LEVEL(p_s_bp) != FREE_LEVEL);
}

/* To gets item head in le form */
void
copy_item_head(struct item_head *p_v_to, const struct item_head *p_v_from)
{

	memcpy(p_v_to, p_v_from, IH_SIZE);
}

/*
 * k1 is pointer to on-disk structure which is stored in little-endian
 * form. k2 is pointer to cpu variable. For key of items of the same
 * object this returns 0.
 * Returns: -1 if key1 < key2, 0 if key1 == key2 or 1 if key1 > key2
 */
/*inline*/ int
comp_short_keys(const struct key *le_key, const struct cpu_key *cpu_key)
{
	const uint32_t *p_s_le_u32, *p_s_cpu_u32;
	int n_key_length = REISERFS_SHORT_KEY_LEN;

	p_s_le_u32  = (const uint32_t *)le_key;
	p_s_cpu_u32 = (const uint32_t *)&cpu_key->on_disk_key;
	for(; n_key_length--; ++p_s_le_u32, ++p_s_cpu_u32) {
		if (le32toh(*p_s_le_u32) < *p_s_cpu_u32)
			return (-1);
		if (le32toh(*p_s_le_u32) > *p_s_cpu_u32)
			return (1);
	}

	return (0);
}

/*
 * k1 is pointer to on-disk structure which is stored in little-endian
 * form. k2 is pointer to cpu variable. Compare keys using all 4 key
 * fields.
 * Returns: -1 if key1 < key2, 0 if key1 = key2 or 1 if key1 > key2
 */
/*inline*/ int
comp_keys(const struct key *le_key, const struct cpu_key *cpu_key)
{
	int retval;

	retval = comp_short_keys(le_key, cpu_key);
	if (retval)
		return retval;

	if (le_key_k_offset(le_key_version(le_key), le_key) <
	    cpu_key_k_offset(cpu_key))
		return (-1);
	if (le_key_k_offset(le_key_version(le_key), le_key) >
	    cpu_key_k_offset(cpu_key))
		return (1);

	if (cpu_key->key_length == 3)
		return (0);

	/* This part is needed only when tail conversion is in progress */
	if (le_key_k_type(le_key_version(le_key), le_key) < 
	    cpu_key_k_type(cpu_key))
		return (-1);

	if (le_key_k_type(le_key_version(le_key), le_key) >
	    cpu_key_k_type(cpu_key))
		return (1);

	return (0);
}

/* Release all buffers in the path. */
void
pathrelse(struct path *p_s_search_path)
{
	struct buf *bp;
	int n_path_offset = p_s_search_path->path_length;

	while (n_path_offset > ILLEGAL_PATH_ELEMENT_OFFSET) {
		bp = PATH_OFFSET_PBUFFER(p_s_search_path, n_path_offset--);
		free(bp->b_data, M_REISERFSPATH);
		free(bp, M_REISERFSPATH);
	}

	p_s_search_path->path_length = ILLEGAL_PATH_ELEMENT_OFFSET;
}

/*
 * This does not say which one is bigger, it only returns 1 if keys
 * are not equal, 0 otherwise
 */
int
comp_le_keys(const struct key *k1, const struct key *k2)
{

	return (memcmp(k1, k2, sizeof(struct key)));
}

/*
 * Binary search toolkit function. Search for an item in the array by
 * the item key.
 * Returns: 1 if found,  0 if not found;
 *          *p_n_pos = number of the searched element if found, else the
 *          number of the first element that is larger than p_v_key.
 */
/*
 * For those not familiar with binary search: n_lbound is the leftmost
 * item that it could be, n_rbound the rightmost item that it could be.
 * We examine the item halfway between n_lbound and n_rbound, and that
 * tells us either that we can increase n_lbound, or decrease n_rbound,
 * or that we have found it, or if n_lbound <= n_rbound that there are
 * no possible items, and we have not found it. With each examination we
 * cut the number of possible items it could be by one more than half
 * rounded down, or we find it.
 */
int
bin_search(const void *p_v_key,  /* Key to search for. */
    const void *p_v_base, /* First item in the array. */
    int p_n_num,          /* Number of items in the array. */
    int p_n_width,        /* Item size in the array. searched. Lest the
			     reader be confused, note that this is crafted
			     as a general function, and when it is applied
			     specifically to the array of item headers in
			     a node, p_n_width is actually the item header
			     size not the item size. */
    int *p_n_pos)         /* Number of the searched for element. */
{
	int n_rbound, n_lbound, n_j;

	for (n_j = ((n_rbound = p_n_num - 1) + (n_lbound = 0)) / 2;
	    n_lbound <= n_rbound; n_j = (n_rbound + n_lbound) / 2) {
		switch (COMP_KEYS((const struct key *)
		    ((const char *)p_v_base + n_j * p_n_width),
		    (const struct cpu_key *)p_v_key)) {
		case -1:
			n_lbound = n_j + 1;
			continue;
		case 1:
			n_rbound = n_j - 1;
			continue;
		case 0:
			*p_n_pos = n_j;
			return (ITEM_FOUND); /* Key found in the array. */
		}
	}

	/*
	 * bin_search did not find given key, it returns position of key,
	 * that is minimal and greater than the given one.
	 */
	*p_n_pos = n_lbound;
	return (ITEM_NOT_FOUND);
}

/*
 * Get delimiting key of the buffer by looking for it in the buffers in
 * the path, starting from the bottom of the path, and going upwards. We
 * must check the path's validity at each step. If the key is not in the
 * path, there is no delimiting key in the tree (buffer is first or last
 * buffer in tree), and in this case we return a special key, either
 * MIN_KEY or MAX_KEY.
 */
const struct key *
get_lkey(const struct path *p_s_chk_path,
    const struct reiserfs_sb_info *p_s_sbi)
{
	struct buf *p_s_parent;
	int n_position, n_path_offset = p_s_chk_path->path_length;

	/* While not higher in path than first element. */
	while (n_path_offset-- > FIRST_PATH_ELEMENT_OFFSET) {
		/* Parent at the path is not in the tree now. */
		if (!B_IS_IN_TREE(p_s_parent =
		    PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset)))
			return (&MAX_KEY);

		/* Check whether position in the parent is correct. */
		if ((n_position = PATH_OFFSET_POSITION(p_s_chk_path,
		    n_path_offset)) > B_NR_ITEMS(p_s_parent))
			return (&MAX_KEY);

		/*
		 * Check whether parent at the path really points to
		 * the child.
		 */
		if (B_N_CHILD_NUM(p_s_parent, n_position) !=
		    (PATH_OFFSET_PBUFFER(p_s_chk_path,
					 n_path_offset + 1)->b_blkno
		     / btodb(p_s_sbi->s_blocksize)))
			return (&MAX_KEY);

		/*
		 * Return delimiting key if position in the parent is not
		 * equal to zero.
		 */
		if (n_position)
			return (B_N_PDELIM_KEY(p_s_parent, n_position - 1));
	}

	/* Return MIN_KEY if we are in the root of the buffer tree. */
	if ((PATH_OFFSET_PBUFFER(p_s_chk_path,
	    FIRST_PATH_ELEMENT_OFFSET)->b_blkno
	    / btodb(p_s_sbi->s_blocksize)) == SB_ROOT_BLOCK(p_s_sbi))
		return (&MIN_KEY);

	return (&MAX_KEY);
}

/* Get delimiting key of the buffer at the path and its right neighbor. */
const struct key *
get_rkey(const struct path *p_s_chk_path,
    const struct reiserfs_sb_info *p_s_sbi)
{
	struct buf *p_s_parent;
	int n_position, n_path_offset = p_s_chk_path->path_length;

	while (n_path_offset-- > FIRST_PATH_ELEMENT_OFFSET) {
		/* Parent at the path is not in the tree now. */
		if (!B_IS_IN_TREE(p_s_parent =
		    PATH_OFFSET_PBUFFER(p_s_chk_path, n_path_offset)))
			return (&MIN_KEY);

		/* Check whether position in the parent is correct. */
		if ((n_position = PATH_OFFSET_POSITION(p_s_chk_path,
		    n_path_offset)) >
		    B_NR_ITEMS(p_s_parent))
			return (&MIN_KEY);

		/*
		 * Check whether parent at the path really points to the
		 * child.
		 */
		if (B_N_CHILD_NUM(p_s_parent, n_position) !=
		    (PATH_OFFSET_PBUFFER(p_s_chk_path,
					 n_path_offset + 1)->b_blkno
		     / btodb(p_s_sbi->s_blocksize)))
			return (&MIN_KEY);

		/*
		 * Return delimiting key if position in the parent is not
		 * the last one.
		 */
		if (n_position != B_NR_ITEMS(p_s_parent))
			return (B_N_PDELIM_KEY(p_s_parent, n_position));
	}

	/* Return MAX_KEY if we are in the root of the buffer tree. */
	if ((PATH_OFFSET_PBUFFER(p_s_chk_path,
	    FIRST_PATH_ELEMENT_OFFSET)->b_blkno
	    / btodb(p_s_sbi->s_blocksize)) == SB_ROOT_BLOCK(p_s_sbi))
		return (&MAX_KEY);

	return (&MIN_KEY);
}

int
reiserfs_check_path(struct path *p)
{

	if (p->path_length != ILLEGAL_PATH_ELEMENT_OFFSET)
		reiserfs_log(LOG_WARNING, "path not properly relsed\n");
	return (0);
}

/*
 * Check whether a key is contained in the tree rooted from a buffer at
 * a path. This works by looking at the left and right delimiting keys
 * for the buffer in the last path_element in the path. These delimiting
 * keys are stored at least one level above that buffer in the tree.
 * If the buffer is the first or last node in the tree order then one
 * of the delimiting keys may be absent, and in this case get_lkey and
 * get_rkey return a special key which is MIN_KEY or MAX_KEY.
 */
static inline int
key_in_buffer(
    struct path *p_s_chk_path,         /* Path which should be checked. */
    const struct cpu_key *p_s_key,     /* Key which should be checked. */
    struct reiserfs_sb_info  *p_s_sbi) /* Super block pointer. */
{

	if (COMP_KEYS(get_lkey(p_s_chk_path, p_s_sbi), p_s_key) == 1)
		/* left delimiting key is bigger, that the key we look for */
		return (0);

	if (COMP_KEYS(get_rkey(p_s_chk_path, p_s_sbi), p_s_key) != 1)
		/* p_s_key must be less than right delimitiing key */
		return (0);

	return (1);
}

#if 0
/* XXX Il ne semble pas y avoir de compteur de référence dans struct buf */
inline void
decrement_bcount(struct buf *p_s_bp)
{

	if (p_s_bp) {
		if (atomic_read(&(p_s_bp->b_count))) {
			put_bh(p_s_bp);
			return;
		}
	}
}
#endif

/* Decrement b_count field of the all buffers in the path. */
void
decrement_counters_in_path(struct path *p_s_search_path)
{

	pathrelse(p_s_search_path);
#if 0
	int n_path_offset = p_s_search_path->path_length;

	while (n_path_offset > ILLEGAL_PATH_ELEMENT_OFFSET) {
		struct buf *bp;

		bp = PATH_OFFSET_PBUFFER(p_s_search_path, n_path_offset--);
		decrement_bcount(bp);
	}

	p_s_search_path->path_length = ILLEGAL_PATH_ELEMENT_OFFSET;
#endif
}

static int
is_leaf(char *buf, int blocksize, struct buf *bp)
{
	struct item_head *ih;
	struct block_head *blkh;
	int used_space, prev_location, i, nr;

	blkh = (struct block_head *)buf;
	if (blkh_level(blkh) != DISK_LEAF_NODE_LEVEL) {
		reiserfs_log(LOG_WARNING, "this should be caught earlier");
		return (0);
	}

	nr = blkh_nr_item(blkh);
	if (nr < 1 || nr >
	    ((blocksize - BLKH_SIZE) / (IH_SIZE + MIN_ITEM_LEN))) {
		/* Item number is too big or too small */
		reiserfs_log(LOG_WARNING, "nr_item seems wrong\n");
		return (0);
	}

	ih = (struct item_head *)(buf + BLKH_SIZE) + nr - 1;
	used_space = BLKH_SIZE + IH_SIZE * nr + (blocksize - ih_location(ih));
	if (used_space != blocksize - blkh_free_space(blkh)) {
		/*
		 * Free space does not match to calculated amount of
		 * use space
		 */
		reiserfs_log(LOG_WARNING, "free space seems wrong\n");
		return (0);
	}

	/* FIXME: it is_leaf will hit performance too much - we may have
	 * return 1 here */

	/* Check tables of item heads */
	ih = (struct item_head *)(buf + BLKH_SIZE);
	prev_location = blocksize;
	for (i = 0; i < nr; i++, ih++) {
		if (le_ih_k_type(ih) == TYPE_ANY) {
			reiserfs_log(LOG_WARNING,
			    "wrong item type for item\n");
			return (0);
		}
		if (ih_location(ih) >= blocksize ||
		    ih_location(ih) < IH_SIZE * nr) {
			reiserfs_log(LOG_WARNING,
			    "item location seems wrong\n");
			return (0);
		}
		if (ih_item_len(ih) < 1 ||
		    ih_item_len(ih) > MAX_ITEM_LEN(blocksize)) {
			reiserfs_log(LOG_WARNING, "item length seems wrong\n");
			return (0);
		}
		if (prev_location - ih_location(ih) != ih_item_len(ih)) {
			reiserfs_log(LOG_WARNING,
			    "item location seems wrong (second one)\n");
			return (0);
		}
		prev_location = ih_location(ih);
	}

	/* One may imagine much more checks */
	return 1;
}

/* Returns 1 if buf looks like an internal node, 0 otherwise */
static int
is_internal(char *buf, int blocksize, struct buf *bp)
{
	int nr, used_space;
	struct block_head *blkh;

	blkh = (struct block_head *)buf;
	nr   = blkh_level(blkh);
	if (nr <= DISK_LEAF_NODE_LEVEL || nr > MAX_HEIGHT) {
		/* This level is not possible for internal nodes */
		reiserfs_log(LOG_WARNING, "this should be caught earlier\n");
		return (0);
	}

	nr = blkh_nr_item(blkh);
	if (nr > (blocksize - BLKH_SIZE - DC_SIZE) / (KEY_SIZE + DC_SIZE)) {
		/*
		 * For internal which is not root we might check min
		 * number of keys
		 */
		reiserfs_log(LOG_WARNING, "number of key seems wrong\n");
		return (0);
	}

	used_space = BLKH_SIZE + KEY_SIZE * nr + DC_SIZE * (nr + 1);
	if (used_space != blocksize - blkh_free_space(blkh)) {
		reiserfs_log(LOG_WARNING,
		    "is_internal: free space seems wrong\n");
		return (0);
	}

	/* One may imagine much more checks */
	return (1);
}

/*
 * Make sure that bh contains formatted node of reiserfs tree of
 * 'level'-th level
 */
static int
is_tree_node(struct buf *bp, int level)
{
	if (B_LEVEL(bp) != level) {
		reiserfs_log(LOG_WARNING,
		    "node level (%d) doesn't match to the "
		    "expected one (%d)\n", B_LEVEL (bp), level);
		return (0);
	}

	if (level == DISK_LEAF_NODE_LEVEL)
		return (is_leaf(bp->b_data, bp->b_bcount, bp));

	return (is_internal(bp->b_data, bp->b_bcount, bp));
}

int
search_by_key(struct reiserfs_sb_info *p_s_sbi,
    const struct cpu_key * p_s_key, /* Key to search. */
    struct path * p_s_search_path,  /* This structure was allocated and
				       initialized by the calling function.
				       It is filled up by this function. */
    int n_stop_level)               /* How far down the tree to search. To
				       stop at leaf level - set to
				       DISK_LEAF_NODE_LEVEL */
{
	int error;
	int n_node_level, n_retval;
	int n_block_number, expected_level, fs_gen;
	struct path_element *p_s_last_element;
	struct buf *p_s_bp, *tmp_bp;

	/*
	 * As we add each node to a path we increase its count. This means that
	 * we must be careful to release all nodes in a path before we either
	 * discard the path struct or re-use the path struct, as we do here.
	 */
	decrement_counters_in_path(p_s_search_path);

	/*
	 * With each iteration of this loop we search through the items in the
	 * current node, and calculate the next current node(next path element)
	 * for the next iteration of this loop...
	 */
	n_block_number = SB_ROOT_BLOCK(p_s_sbi);
	expected_level = -1;

	reiserfs_log(LOG_DEBUG, "root block: #%d\n", n_block_number);

	while (1) {
		/* Prep path to have another element added to it. */
		reiserfs_log(LOG_DEBUG, "path element #%d\n",
		    p_s_search_path->path_length);
		p_s_last_element = PATH_OFFSET_PELEMENT(p_s_search_path,
		    ++p_s_search_path->path_length);
		fs_gen = get_generation(p_s_sbi);

		/*
		 * Read the next tree node, and set the last element in the
		 * path to have a pointer to it.
		 */
		reiserfs_log(LOG_DEBUG, "reading block #%d\n",
		    n_block_number);
		if ((error = bread(p_s_sbi->s_devvp,
		    n_block_number * btodb(p_s_sbi->s_blocksize),
		    p_s_sbi->s_blocksize, NOCRED, &tmp_bp)) != 0) {
			reiserfs_log(LOG_DEBUG, "error reading block\n");
			p_s_search_path->path_length--;
			pathrelse(p_s_search_path);
			return (IO_ERROR);
		}
		reiserfs_log(LOG_DEBUG, "blkno = %ju, lblkno = %ju\n",
		    (intmax_t)tmp_bp->b_blkno, (intmax_t)tmp_bp->b_lblkno);

		/*
		 * As i didn't found a way to handle the lock correctly,
		 * i copy the data into a fake buffer
		 */
		reiserfs_log(LOG_DEBUG, "allocating p_s_bp\n");
		p_s_bp = malloc(sizeof *p_s_bp, M_REISERFSPATH, M_WAITOK);
		if (!p_s_bp) {
			reiserfs_log(LOG_DEBUG, "error allocating memory\n");
			p_s_search_path->path_length--;
			pathrelse(p_s_search_path);
			brelse(tmp_bp);
			return (IO_ERROR);
		}
		reiserfs_log(LOG_DEBUG, "copying struct buf\n");
		bcopy(tmp_bp, p_s_bp, sizeof(struct buf));

		reiserfs_log(LOG_DEBUG, "allocating p_s_bp->b_data\n");
		p_s_bp->b_data = malloc(p_s_sbi->s_blocksize,
		    M_REISERFSPATH, M_WAITOK);
		if (!p_s_bp->b_data) {
			reiserfs_log(LOG_DEBUG, "error allocating memory\n");
			p_s_search_path->path_length--;
			pathrelse(p_s_search_path);
			free(p_s_bp, M_REISERFSPATH);
			brelse(tmp_bp);
			return (IO_ERROR);
		}
		reiserfs_log(LOG_DEBUG, "copying buffer data\n");
		bcopy(tmp_bp->b_data, p_s_bp->b_data, p_s_sbi->s_blocksize);
		brelse(tmp_bp);
		tmp_bp = NULL;

		reiserfs_log(LOG_DEBUG, "...done\n");
		p_s_last_element->pe_buffer = p_s_bp;

		if (expected_level == -1)
			expected_level = SB_TREE_HEIGHT(p_s_sbi);
		expected_level--;
		reiserfs_log(LOG_DEBUG, "expected level: %d (%d)\n",
		    expected_level, SB_TREE_HEIGHT(p_s_sbi));

		/* XXX */ 
		/*
		 * It is possible that schedule occurred. We must check
		 * whether the key to search is still in the tree rooted
		 * from the current buffer. If not then repeat search
		 * from the root.
		 */
		if (fs_changed(fs_gen, p_s_sbi) &&
		    (!B_IS_IN_TREE(p_s_bp) ||
		     B_LEVEL(p_s_bp) != expected_level ||
		     !key_in_buffer(p_s_search_path, p_s_key, p_s_sbi))) {
			reiserfs_log(LOG_DEBUG,
			    "the key isn't in the tree anymore\n");
			decrement_counters_in_path(p_s_search_path);

			/*
			 * Get the root block number so that we can repeat
			 * the search starting from the root.
			 */
			n_block_number = SB_ROOT_BLOCK(p_s_sbi);
			expected_level = -1;

			/* Repeat search from the root */
			continue;
		}

		/*
		 * Make sure, that the node contents look like a node of
		 * certain level
		 */
		if (!is_tree_node(p_s_bp, expected_level)) {
			reiserfs_log(LOG_WARNING,
			    "invalid format found in block %ju. Fsck?",
			    (intmax_t)p_s_bp->b_blkno);
			pathrelse (p_s_search_path);
			return (IO_ERROR);
		}

		/* Ok, we have acquired next formatted node in the tree */
		n_node_level = B_LEVEL(p_s_bp);
		reiserfs_log(LOG_DEBUG, "block info:\n");
		reiserfs_log(LOG_DEBUG, "  node level:  %d\n",
		    n_node_level);
		reiserfs_log(LOG_DEBUG, "  nb of items: %d\n",
		    B_NR_ITEMS(p_s_bp));
		reiserfs_log(LOG_DEBUG, "  free space:  %d bytes\n",
		    B_FREE_SPACE(p_s_bp));
		reiserfs_log(LOG_DEBUG, "bin_search with :\n"
		    "  p_s_key = (objectid=%d, dirid=%d)\n"
		    "  B_NR_ITEMS(p_s_bp) = %d\n"
		    "  p_s_last_element->pe_position = %d (path_length = %d)\n",
		    p_s_key->on_disk_key.k_objectid,
		    p_s_key->on_disk_key.k_dir_id,
		    B_NR_ITEMS(p_s_bp),
		    p_s_last_element->pe_position,
		    p_s_search_path->path_length);
		n_retval = bin_search(p_s_key, B_N_PITEM_HEAD(p_s_bp, 0),
		    B_NR_ITEMS(p_s_bp),
		    (n_node_level == DISK_LEAF_NODE_LEVEL) ? IH_SIZE : KEY_SIZE,
		    &(p_s_last_element->pe_position));
		reiserfs_log(LOG_DEBUG, "bin_search result: %d\n",
		    n_retval);
		if (n_node_level == n_stop_level) {
			reiserfs_log(LOG_DEBUG, "stop level reached (%s)\n",
			    n_retval == ITEM_FOUND ? "found" : "not found");
			return (n_retval);
		}

		/* We are not in the stop level */
		if (n_retval == ITEM_FOUND)
			/*
			 * Item has been found, so we choose the pointer
			 * which is to the right of the found one
			 */
			p_s_last_element->pe_position++;

		/*
		 * If item was not found we choose the position which is
		 * to the left of the found item. This requires no code,
		 * bin_search did it already.
		 */

		/*
		 * So we have chosen a position in the current node which
		 * is an internal node. Now we calculate child block number
		 * by position in the node.
		 */
		n_block_number = B_N_CHILD_NUM(p_s_bp,
		    p_s_last_element->pe_position);
	}

	reiserfs_log(LOG_DEBUG, "done\n");
	return (0);
}

/*
 * Form the path to an item and position in this item which contains
 * file byte defined by p_s_key. If there is no such item corresponding
 * to the key, we point the path to the item with maximal key less than
 * p_s_key, and *p_n_pos_in_item is set to one past the last entry/byte
 * in the item. If searching for entry in a directory item, and it is
 * not found, *p_n_pos_in_item is set to one entry more than the entry
 * with maximal key which is less than the sought key.
 *
 * Note that if there is no entry in this same node which is one more,
 * then we point to an imaginary entry. For direct items, the position
 * is in units of bytes, for indirect items the position is in units
 * of blocknr entries, for directory items the position is in units of
 * directory entries.
 */

/* The function is NOT SCHEDULE-SAFE! */
int
search_for_position_by_key(struct reiserfs_sb_info *p_s_sbi,
    const struct cpu_key *p_cpu_key, /* Key to search (cpu variable) */
    struct path *p_s_search_path)    /* Filled up by this function.  */
{
	int retval, n_blk_size;
	off_t item_offset, offset;
	struct item_head *p_le_ih; /* Pointer to on-disk structure */
	struct reiserfs_dir_entry de;

	/* If searching for directory entry. */
	if (is_direntry_cpu_key(p_cpu_key))
		return (search_by_entry_key(p_s_sbi, p_cpu_key,
		    p_s_search_path, &de));

	/* If not searching for directory entry. */

	/* If item is found. */
	retval = search_item(p_s_sbi, p_cpu_key, p_s_search_path);
	if (retval == IO_ERROR)
		return (retval);
	if (retval == ITEM_FOUND) {
		if (ih_item_len(B_N_PITEM_HEAD(
		    PATH_PLAST_BUFFER(p_s_search_path),
		    PATH_LAST_POSITION(p_s_search_path))) == 0) {
			reiserfs_log(LOG_WARNING, "item length equals zero\n");
		}

		pos_in_item(p_s_search_path) = 0;
		return (POSITION_FOUND);
	}

	if (PATH_LAST_POSITION(p_s_search_path) == 0) {
		reiserfs_log(LOG_WARNING, "position equals zero\n");
	}

	/* Item is not found. Set path to the previous item. */
	p_le_ih = B_N_PITEM_HEAD(PATH_PLAST_BUFFER(p_s_search_path),
	    --PATH_LAST_POSITION(p_s_search_path));
	n_blk_size = p_s_sbi->s_blocksize;

	if (comp_short_keys(&(p_le_ih->ih_key), p_cpu_key)) {
		return (FILE_NOT_FOUND);
	}

	item_offset = le_ih_k_offset(p_le_ih);
	offset = cpu_key_k_offset(p_cpu_key);

	/* Needed byte is contained in the item pointed to by the path.*/
	if (item_offset <= offset &&
	    item_offset + op_bytes_number(p_le_ih, n_blk_size) > offset) {
		pos_in_item(p_s_search_path) = offset - item_offset;
		if (is_indirect_le_ih(p_le_ih)) {
			pos_in_item(p_s_search_path) /= n_blk_size;
		}
		return (POSITION_FOUND);
	}

	/* Needed byte is not contained in the item pointed to by the
	 * path. Set pos_in_item out of the item. */
	if (is_indirect_le_ih(p_le_ih))
		pos_in_item(p_s_search_path) =
		    ih_item_len(p_le_ih) / UNFM_P_SIZE;
	else
		pos_in_item(p_s_search_path) =
		    ih_item_len(p_le_ih);

	return (POSITION_NOT_FOUND);
}
