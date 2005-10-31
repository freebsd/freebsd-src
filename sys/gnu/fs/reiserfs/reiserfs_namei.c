/*-
 * Copyright 2000 Hans Reiser
 * See README for licensing and copyright details
 * 
 * Ported to FreeBSD by Jean-Sébastien Pédron <jspedron@club-internet.fr>
 * 
 * $FreeBSD$
 */

#include <gnu/fs/reiserfs/reiserfs_fs.h>

static int	reiserfs_find_entry(struct reiserfs_node *dp,
    const char *name, int namelen,
    struct path * path_to_entry, struct reiserfs_dir_entry *de);

MALLOC_DEFINE(M_REISERFSCOOKIES, "reiserfs_cookies",
    "ReiserFS VOP_READDIR cookies");

/* -------------------------------------------------------------------
 * Lookup functions
 * -------------------------------------------------------------------*/

int
reiserfs_lookup(struct vop_cachedlookup_args *ap)
{
	int error, retval;
	struct vnode *vdp         = ap->a_dvp;
	struct vnode **vpp        = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;

	int flags         = cnp->cn_flags;
	struct thread *td = cnp->cn_thread;
	struct cpu_key *saved_ino;

	struct vnode *vp;
	struct vnode *pdp;  /* Saved dp during symlink work */
	struct reiserfs_node *dp;
	struct reiserfs_dir_entry de;
	INITIALIZE_PATH(path_to_entry);

	char c = cnp->cn_nameptr[cnp->cn_namelen];
	cnp->cn_nameptr[cnp->cn_namelen] = '\0';
	reiserfs_log(LOG_DEBUG, "looking for `%s', %ld (%s)\n",
	    cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_pnbuf);
	cnp->cn_nameptr[cnp->cn_namelen] = c;

	vp = NULL;
	dp = VTOI(vdp);

	if (REISERFS_MAX_NAME(dp->i_reiserfs->s_blocksize) < cnp->cn_namelen)
		return (ENAMETOOLONG);

	reiserfs_log(LOG_DEBUG, "searching entry\n");
	de.de_gen_number_bit_string = 0;
	retval = reiserfs_find_entry(dp, cnp->cn_nameptr, cnp->cn_namelen,
	    &path_to_entry, &de);
	pathrelse(&path_to_entry);

	if (retval == NAME_FOUND) {
		reiserfs_log(LOG_DEBUG, "found\n");
	} else {
		reiserfs_log(LOG_DEBUG, "not found\n");
	}

	if (retval == NAME_FOUND) {
#if 0
		/* Hide the .reiserfs_priv directory */
		if (reiserfs_xattrs(dp->i_reiserfs) &&
		    !old_format_only(dp->i_reiserfs) &&
		    REISERFS_SB(dp->i_reiserfs)->priv_root &&
		    REISERFS_SB(dp->i_reiserfs)->priv_root->d_inode &&
		    de.de_objectid == le32toh(INODE_PKEY(REISERFS_SB(
		    dp->i_reiserfs)->priv_root->d_inode)->k_objectid)) {
			return (EACCES);
		}
#endif

		reiserfs_log(LOG_DEBUG, "reading vnode\n");
		pdp = vdp;
		if (flags & ISDOTDOT) {
			saved_ino = (struct cpu_key *)&(de.de_dir_id);
			VOP_UNLOCK(pdp, 0, td);
			error = reiserfs_iget(vdp->v_mount,
			    saved_ino, &vp, td);
			vn_lock(pdp, LK_EXCLUSIVE | LK_RETRY, td);
			if (error != 0)
				return (error);
			*vpp = vp;
		} else if (de.de_objectid == dp->i_number &&
		    de.de_dir_id == dp->i_ino) {
			VREF(vdp); /* We want ourself, ie "." */
			*vpp = vdp;
		} else {
			if ((error = reiserfs_iget(vdp->v_mount,
			    (struct cpu_key *)&(de.de_dir_id), &vp, td)) != 0)
				return (error);
			*vpp = vp;
		}

		/*
		 * Propogate the priv_object flag so we know we're in the
		 * priv tree
		 */
		/*if (is_reiserfs_priv_object(dir))
			REISERFS_I(inode)->i_flags |= i_priv_object;*/
	} else {
		if (retval == IO_ERROR) {
			reiserfs_log(LOG_DEBUG, "IO error\n");
			return (EIO);
		}

		return (ENOENT);
	}

	/* Insert name into cache if appropriate. */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);

	reiserfs_log(LOG_DEBUG, "done\n");
	return (0);
}

extern struct key MIN_KEY;

int
reiserfs_readdir(struct vop_readdir_args  /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */*ap)
{
	int error = 0;
	struct dirent dstdp;
	struct uio *uio = ap->a_uio;

	off_t next_pos;
	struct buf *bp;
	struct item_head *ih;
	struct cpu_key pos_key;
	const struct key *rkey;
	struct reiserfs_node *ip;
	struct reiserfs_dir_entry de;
	INITIALIZE_PATH(path_to_entry);
	int entry_num, item_num, search_res;

	/* The NFS part */
	int ncookies = 0;
	u_long *cookies = NULL;

	/*
	 * Form key for search the next directory entry using f_pos field of
	 * file structure
	 */
	ip = VTOI(ap->a_vp);
	make_cpu_key(&pos_key,
	    ip, uio->uio_offset ? uio->uio_offset : DOT_OFFSET,
	    TYPE_DIRENTRY, 3);
	next_pos = cpu_key_k_offset(&pos_key);

	reiserfs_log(LOG_DEBUG, "listing entries for "
	    "(objectid=%d, dirid=%d)\n",
	    pos_key.on_disk_key.k_objectid, pos_key.on_disk_key.k_dir_id);
	reiserfs_log(LOG_DEBUG, "uio_offset = %jd, uio_resid = %d\n",
	    (intmax_t)uio->uio_offset, uio->uio_resid);

	if (ap->a_ncookies && ap->a_cookies) {
		cookies = (u_long *)malloc(
		    uio->uio_resid / 16 * sizeof(u_long),
		    M_REISERFSCOOKIES, M_WAITOK);
	}

	while (1) {
		//research:
		/*
		 * Search the directory item, containing entry with
		 * specified key
		 */
		reiserfs_log(LOG_DEBUG, "search directory to read\n");
		search_res = search_by_entry_key(ip->i_reiserfs, &pos_key,
		    &path_to_entry, &de);
		if (search_res == IO_ERROR) {
			error = EIO;
			goto out;
		}

		entry_num = de.de_entry_num;
		item_num  = de.de_item_num;
		bp = de.de_bp;
		ih = de.de_ih;

		if (search_res == POSITION_FOUND ||
		    entry_num < I_ENTRY_COUNT(ih)) {
			/*
			 * Go through all entries in the directory item
			 * beginning from the entry, that has been found.
			 */
			struct reiserfs_de_head *deh = B_I_DEH(bp, ih) +
			    entry_num;

			if (ap->a_ncookies == NULL) {
				cookies = NULL;
			} else {
				//ncookies = 
			}

			reiserfs_log(LOG_DEBUG,
			    "walking through directory entries\n");
			for (; entry_num < I_ENTRY_COUNT(ih);
			    entry_num++, deh++) {
				int d_namlen;
				char *d_name;
				off_t d_off;
				ino_t d_ino;

				if (!de_visible(deh)) {
					/* It is hidden entry */
					continue;
				}

				d_namlen = entry_length(bp, ih, entry_num);
				d_name   = B_I_DEH_ENTRY_FILE_NAME(bp, ih, deh);
				if (!d_name[d_namlen - 1])
					d_namlen = strlen(d_name);
				reiserfs_log(LOG_DEBUG, "  - `%s' (len=%d)\n",
				    d_name, d_namlen);

				if (d_namlen > REISERFS_MAX_NAME(
				    ip->i_reiserfs->s_blocksize)) {
					/* Too big to send back to VFS */
					continue;
				}

#if 0
				/* Ignore the .reiserfs_priv entry */
				if (reiserfs_xattrs(ip->i_reiserfs) &&
				    !old_format_only(ip->i_reiserfs) &&
				    filp->f_dentry == ip->i_reiserfs->s_root &&
				    REISERFS_SB(ip->i_reiserfs)->priv_root &&
				    REISERFS_SB(ip->i_reiserfs)->priv_root->d_inode &&
				    deh_objectid(deh) ==
				    le32toh(INODE_PKEY(REISERFS_SB(
				    ip->i_reiserfs)->priv_root->d_inode)->k_objectid)) {
					continue;
				}
#endif

				d_off = deh_offset(deh);
				d_ino = deh_objectid(deh);
				uio->uio_offset = d_off;

				/* Copy to user land */
				dstdp.d_fileno = d_ino;
				dstdp.d_type   = DT_UNKNOWN;
				dstdp.d_namlen = d_namlen;
				dstdp.d_reclen = GENERIC_DIRSIZ(&dstdp);
				bcopy(d_name, dstdp.d_name, dstdp.d_namlen);
				bzero(dstdp.d_name + dstdp.d_namlen,
				    dstdp.d_reclen -
				    offsetof(struct dirent, d_name) -
				    dstdp.d_namlen);

				if (d_namlen > 0) {
					if (dstdp.d_reclen <= uio->uio_resid) {
						reiserfs_log(LOG_DEBUG, "     copying to user land\n");
						error = uiomove(&dstdp,
						    dstdp.d_reclen, uio);
						if (error)
							goto end;
						if (cookies != NULL) {
							cookies[ncookies] =
							    d_off;
							ncookies++;
						}
					} else
						break;
				} else {
					error = EIO;
					break;
				}

				next_pos = deh_offset(deh) + 1;
			}
			reiserfs_log(LOG_DEBUG, "...done\n");
		}

		reiserfs_log(LOG_DEBUG, "checking item num (%d == %d ?)\n",
		    item_num, B_NR_ITEMS(bp) - 1);
		if (item_num != B_NR_ITEMS(bp) - 1) {
			/* End of directory has been reached */
			reiserfs_log(LOG_DEBUG, "end reached\n");
			if (ap->a_eofflag)
				*ap->a_eofflag = 1;
			goto end;
		}

		/*
		 * Item we went through is last item of node. Using right
		 * delimiting key check is it directory end
		 */
		reiserfs_log(LOG_DEBUG, "get right key\n");
		rkey = get_rkey(&path_to_entry, ip->i_reiserfs);
		reiserfs_log(LOG_DEBUG, "right key = (objectid=%d, dirid=%d)\n",
		    rkey->k_objectid, rkey->k_dir_id);

		reiserfs_log(LOG_DEBUG, "compare it to MIN_KEY\n");
		reiserfs_log(LOG_DEBUG, "MIN KEY = (objectid=%d, dirid=%d)\n",
		    MIN_KEY.k_objectid, MIN_KEY.k_dir_id);
		if (comp_le_keys(rkey, &MIN_KEY) == 0) {
			/* Set pos_key to key, that is the smallest and greater
			 * that key of the last entry in the item */
			reiserfs_log(LOG_DEBUG, "continuing on the right\n");
			set_cpu_key_k_offset(&pos_key, next_pos);
			continue;
		}

		reiserfs_log(LOG_DEBUG, "compare it to pos_key\n");
		reiserfs_log(LOG_DEBUG, "pos key = (objectid=%d, dirid=%d)\n",
		    pos_key.on_disk_key.k_objectid,
		    pos_key.on_disk_key.k_dir_id);
		if (COMP_SHORT_KEYS(rkey, &pos_key)) {
			/* End of directory has been reached */
			reiserfs_log(LOG_DEBUG, "end reached (right)\n");
			if (ap->a_eofflag)
				*ap->a_eofflag = 1;
			goto end;
		}

		/* Directory continues in the right neighboring block */
		reiserfs_log(LOG_DEBUG, "continuing with a new offset\n");
		set_cpu_key_k_offset(&pos_key,
		    le_key_k_offset(KEY_FORMAT_3_5, rkey));
		reiserfs_log(LOG_DEBUG,
		    "new pos key = (objectid=%d, dirid=%d)\n",
		    pos_key.on_disk_key.k_objectid,
		    pos_key.on_disk_key.k_dir_id);
	}

end:
	uio->uio_offset = next_pos;
	pathrelse(&path_to_entry);
	reiserfs_check_path(&path_to_entry);
out:
	if (error && cookies != NULL) {
		free(cookies, M_REISERFSCOOKIES);
	} else if (ap->a_ncookies != NULL && ap->a_cookies != NULL) {
		*ap->a_ncookies = ncookies;
		*ap->a_cookies  = cookies;
	}
	return (error);
}

/* -------------------------------------------------------------------
 * Functions from linux/fs/reiserfs/namei.c
 * -------------------------------------------------------------------*/


/*
 * Directory item contains array of entry headers. This performs binary
 * search through that array.
 */
static int
bin_search_in_dir_item(struct reiserfs_dir_entry *de, off_t off)
{
	struct item_head *ih = de->de_ih;
	struct reiserfs_de_head *deh = de->de_deh;
	int rbound, lbound, j;

	lbound = 0;
	rbound = I_ENTRY_COUNT(ih) - 1;

	for (j = (rbound + lbound) / 2; lbound <= rbound;
	    j = (rbound + lbound) / 2) {
		if (off < deh_offset(deh + j)) {
			rbound = j - 1;
			continue;
		}
		if (off > deh_offset(deh + j)) {
			lbound = j + 1;
			continue;
		}

		/* This is not name found, but matched third key component */
		de->de_entry_num = j;
		return (NAME_FOUND);
	}

	de->de_entry_num = lbound;
	return (NAME_NOT_FOUND);
}

/*
 * Comment?  Maybe something like set de to point to what the path
 * points to?
 */
static inline void
set_de_item_location(struct reiserfs_dir_entry *de, struct path *path)
{

	de->de_bp       = get_last_bp(path);
	de->de_ih       = get_ih(path);
	de->de_deh      = B_I_DEH(de->de_bp, de->de_ih);
	de->de_item_num = PATH_LAST_POSITION(path);
}

/*
 * de_bh, de_ih, de_deh (points to first element of array), de_item_num
 * is set
 */
inline void
set_de_name_and_namelen(struct reiserfs_dir_entry *de)
{
	struct reiserfs_de_head *deh = de->de_deh + de->de_entry_num;

	if (de->de_entry_num >= ih_entry_count(de->de_ih)) {
		reiserfs_log(LOG_DEBUG, "BUG\n");
		return;
	}

	de->de_entrylen = entry_length(de->de_bp, de->de_ih, de->de_entry_num);
	de->de_namelen  = de->de_entrylen - (de_with_sd(deh) ? SD_SIZE : 0);
	de->de_name     = B_I_PITEM(de->de_bp, de->de_ih) + deh_location(deh);
	if (de->de_name[de->de_namelen - 1] == 0)
		de->de_namelen = strlen(de->de_name);
}

/* What entry points to */
static inline void
set_de_object_key(struct reiserfs_dir_entry *de)
{

	if (de->de_entry_num >= ih_entry_count(de->de_ih)) {
		reiserfs_log(LOG_DEBUG, "BUG\n");
		return;
	}
	de->de_dir_id   = deh_dir_id(&(de->de_deh[de->de_entry_num]));
	de->de_objectid = deh_objectid(&(de->de_deh[de->de_entry_num]));
}

static inline void
store_de_entry_key(struct reiserfs_dir_entry *de)
{
	struct reiserfs_de_head *deh = de->de_deh + de->de_entry_num;

	if (de->de_entry_num >= ih_entry_count(de->de_ih)) {
		reiserfs_log(LOG_DEBUG, "BUG\n"); 
		return;
	}

	/* Store key of the found entry */
	de->de_entry_key.version = KEY_FORMAT_3_5;
	de->de_entry_key.on_disk_key.k_dir_id =
	    le32toh(de->de_ih->ih_key.k_dir_id);
	de->de_entry_key.on_disk_key.k_objectid =
	    le32toh(de->de_ih->ih_key.k_objectid);
	set_cpu_key_k_offset(&(de->de_entry_key), deh_offset(deh));
	set_cpu_key_k_type(&(de->de_entry_key), TYPE_DIRENTRY);
}

/*
 * We assign a key to each directory item, and place multiple entries in
 * a single directory item. A directory item has a key equal to the key
 * of the first directory entry in it.
 *
 * This function first calls search_by_key, then, if item whose first
 * entry matches is not found it looks for the entry inside directory
 * item found by search_by_key. Fills the path to the entry, and to the
 * entry position in the item
 */
int
search_by_entry_key(struct reiserfs_sb_info *sbi,
    const struct cpu_key *key, struct path *path,
    struct reiserfs_dir_entry *de)
{
	int retval;

	reiserfs_log(LOG_DEBUG, "searching in (objectid=%d,dirid=%d)\n",
	    key->on_disk_key.k_objectid, key->on_disk_key.k_dir_id);
	retval = search_item(sbi, key, path);
	switch (retval) {
	case ITEM_NOT_FOUND:
		if (!PATH_LAST_POSITION(path)) {
			reiserfs_log(LOG_DEBUG,
			    "search_by_key returned item position == 0");
			pathrelse(path);
			return (IO_ERROR);
		}
		PATH_LAST_POSITION(path)--;
		reiserfs_log(LOG_DEBUG, "search_by_key did not found it\n");
		break;
	case ITEM_FOUND:
		reiserfs_log(LOG_DEBUG, "search_by_key found it\n");
		break;
	case IO_ERROR:
		return (retval);
	default:
		pathrelse(path);
		reiserfs_log(LOG_DEBUG, "no path to here");
		return (IO_ERROR);
	}

	reiserfs_log(LOG_DEBUG, "set item location\n");
	set_de_item_location(de, path);

	/*
	 * Binary search in directory item by third component of the
	 * key. Sets de->de_entry_num of de
	 */
	reiserfs_log(LOG_DEBUG, "bin_search_in_dir_item\n");
	retval = bin_search_in_dir_item(de, cpu_key_k_offset(key));
	path->pos_in_item = de->de_entry_num;
	if (retval != NAME_NOT_FOUND) {
		/*
		 * Ugly, but rename needs de_bp, de_deh, de_name, de_namelen,
		 * de_objectid set
		 */
		set_de_name_and_namelen(de);
		set_de_object_key(de);
		reiserfs_log(LOG_DEBUG, "set (objectid=%d,dirid=%d)\n",
		    de->de_objectid, de->de_dir_id);
	}

	return (retval);
}

static uint32_t
get_third_component(struct reiserfs_sb_info *sbi, const char *name, int len)
{
	uint32_t res;

	if (!len || (len == 1 && name[0] == '.'))
		return (DOT_OFFSET);

	if (len == 2 && name[0] == '.' && name[1] == '.')
		return (DOT_DOT_OFFSET);

	res = REISERFS_SB(sbi)->s_hash_function(name, len);

	/* Take bits from 7-th to 30-th including both bounds */
	res = GET_HASH_VALUE(res);
	if (res == 0)
		/*
		 * Needed to have no names before "." and ".." those have hash
		 * value == 0 and generation counters 1 and 2 accordingly
		 */
		res = 128;

	return (res + MAX_GENERATION_NUMBER);
}

static int
reiserfs_match(struct reiserfs_dir_entry *de, const char *name, int namelen)
{
	int retval = NAME_NOT_FOUND;

	if ((namelen == de->de_namelen) &&
	    !memcmp(de->de_name, name, de->de_namelen))
		retval = (de_visible(de->de_deh + de->de_entry_num) ?
		    NAME_FOUND : NAME_FOUND_INVISIBLE);

	return (retval);
}

/*
 * de's de_bh, de_ih, de_deh, de_item_num, de_entry_num are set already
 * Used when hash collisions exist
 */
static int
linear_search_in_dir_item(struct cpu_key *key, struct reiserfs_dir_entry *de,
    const char *name, int namelen)
{
	int i;
	int retval;
	struct reiserfs_de_head * deh = de->de_deh;

	i = de->de_entry_num;

	if (i == I_ENTRY_COUNT(de->de_ih) ||
	    GET_HASH_VALUE(deh_offset(deh + i)) !=
	    GET_HASH_VALUE(cpu_key_k_offset(key))) {
		i--;
	}

	/*RFALSE( de->de_deh != B_I_DEH (de->de_bh, de->de_ih),
	  "vs-7010: array of entry headers not found");*/

	deh += i;

	for (; i >= 0; i--, deh--) {
		if (GET_HASH_VALUE(deh_offset(deh)) !=
		    GET_HASH_VALUE(cpu_key_k_offset(key))) {
			/*
			 * Hash value does not match, no need to check
			 * whole name
			 */
			reiserfs_log(LOG_DEBUG, "name `%s' not found\n", name);
			return (NAME_NOT_FOUND);
		}

		/* Mark that this generation number is used */
		if (de->de_gen_number_bit_string)
			set_bit(GET_GENERATION_NUMBER(deh_offset(deh)),
			    (unsigned long *)de->de_gen_number_bit_string);

		/* Calculate pointer to name and namelen */
		de->de_entry_num = i;
		set_de_name_and_namelen(de);

		if ((retval = reiserfs_match(de, name, namelen)) !=
		    NAME_NOT_FOUND) {
			/*
			 * de's de_name, de_namelen, de_recordlen are set.
			 * Fill the rest:
			 */
			/* key of pointed object */
			set_de_object_key(de);
			store_de_entry_key(de);

			/* retval can be NAME_FOUND or NAME_FOUND_INVISIBLE */
			reiserfs_log(LOG_DEBUG,
			    "reiserfs_match answered `%d'\n",
			    retval);
			return (retval);
		}
	}

	if (GET_GENERATION_NUMBER(le_ih_k_offset(de->de_ih)) == 0)
		/*
		 * We have reached left most entry in the node. In common
		 * we have to go to the left neighbor, but if generation
		 * counter is 0 already, we know for sure, that there is
		 * no name with the same hash value
		 */
		/* FIXME: this work correctly only because hash value can
		 * not be 0. Btw, in case of Yura's hash it is probably
		 * possible, so, this is a bug
		 */
		return (NAME_NOT_FOUND);

	/*RFALSE(de->de_item_num,
	    "vs-7015: two diritems of the same directory in one node?");*/

	return (GOTO_PREVIOUS_ITEM);
}

/*
 * May return NAME_FOUND, NAME_FOUND_INVISIBLE, NAME_NOT_FOUND
 * FIXME: should add something like IOERROR
 */
static int
reiserfs_find_entry(struct reiserfs_node *dp, const char *name, int namelen,
    struct path * path_to_entry, struct reiserfs_dir_entry *de)
{
	struct cpu_key key_to_search;
	int retval;

	if (namelen > REISERFS_MAX_NAME(dp->i_reiserfs->s_blocksize))
		return NAME_NOT_FOUND;

	/* We will search for this key in the tree */
	make_cpu_key(&key_to_search, dp,
	    get_third_component(dp->i_reiserfs, name, namelen),
	    TYPE_DIRENTRY, 3);

	while (1) {
		reiserfs_log(LOG_DEBUG, "search by entry key\n");
		retval = search_by_entry_key(dp->i_reiserfs, &key_to_search,
		    path_to_entry, de);
		if (retval == IO_ERROR) {
			reiserfs_log(LOG_DEBUG, "IO error in %s\n",
			    __FUNCTION__);
			return IO_ERROR;
		}

		/* Compare names for all entries having given hash value */
		reiserfs_log(LOG_DEBUG, "linear search for `%s'\n", name);
		retval = linear_search_in_dir_item(&key_to_search, de,
		    name, namelen);
		if (retval != GOTO_PREVIOUS_ITEM) {
			/*
			 * There is no need to scan directory anymore.
			 * Given entry found or does not exist
			 */
			reiserfs_log(LOG_DEBUG, "linear search returned "
			    "(objectid=%d,dirid=%d)\n",
			    de->de_objectid, de->de_dir_id);
			path_to_entry->pos_in_item = de->de_entry_num;
			return retval;
		}

		/*
		 * There is left neighboring item of this directory and
		 * given entry can be there
		 */
		set_cpu_key_k_offset(&key_to_search,
		    le_ih_k_offset(de->de_ih) - 1);
		pathrelse(path_to_entry);  
	} /* while (1) */
}
