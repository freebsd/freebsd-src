#include <sys/llo_hash.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/time.h>

struct llo_hash *
llo_hash_init(int nelements, 
	      struct malloc_type *typ, 
	      llo_hashfunc hf,
	      llo_comparefunc cf,
	      llo_freefunc ff,
	      size_t keysz,
	      int llo_flags)
{
	struct llo_hash *tbl=NULL;

	tbl = malloc(sizeof(struct llo_hash), typ, 
		     ((llo_flags & LLO_FLAGS_NOWAIT) ? M_NOWAIT : M_WAITOK));
	if (tbl == NULL) {
		goto out_err;
	}
	memset(tbl, 0, sizeof(struct llo_hash));
	tbl->llo_hf = hf;
	tbl->llo_cf = cf;
        tbl->llo_ff = ff;
	tbl->llo_typ = typ;
	callout_init(&tbl->lazy_clist_tmr, 1);
	LIST_INIT(&tbl->lazy_clist);
	LLO_MMTX_INIT(tbl);
	/* Setup our flags */
	if (llo_flags & LLO_FLAGS_NOWAIT) {
		tbl->table_flags |= LLO_IFLAG_NOWAIT;
	}
	if (llo_flags & LLO_FLAGS_MULTI_MTX) {
		tbl->table_flags |= LLO_IFLAG_MMTX;
	}
	if (llo_flags & LLO_FLAGS_MIN_U64) {
		tbl->table_flags |= LLO_IFLAG_MINU64;
	}
	/* Now the mallocs */
	tbl->llo_epoch_start = counter_u64_alloc(((llo_flags & LLO_FLAGS_NOWAIT) ? M_NOWAIT : M_WAITOK));
	if (tbl->llo_epoch_start == NULL) {
		goto out_err;
	}
	tbl->llo_epoch_end = counter_u64_alloc(((llo_flags & LLO_FLAGS_NOWAIT) ? M_NOWAIT : M_WAITOK));
	if (tbl->llo_epoch_end == NULL) {
		goto out_err;
	}
	tbl->llo_ht = hashinit_flags(nelements, tbl->llo_typ, &tbl->llo_hashmod,
				     ((llo_flags & LLO_FLAGS_NOWAIT) ? M_NOWAIT : M_WAITOK));
	if (tbl->llo_ht == NULL) {
		goto out_err;
	}
	/* Now what about the mutex 1 or many? */
	if (tbl->table_flags & LLO_IFLAG_MMTX) {
		tbl->llo_hmtx = malloc(sizeof(struct mtx), tbl->llo_typ, 
				       ((llo_flags & LLO_FLAGS_NOWAIT) ? M_NOWAIT : M_WAITOK));
		if (tbl->llo_hmtx == NULL) {
			goto out_err;
		}
		LLO_MTX_INIT(tbl->llo_hmtx, 0);
	} else {
		size_t sz;
		int i;
		sz = (sizeof(struct mtx) * (tbl->llo_hashmod+1));
		tbl->llo_hmtx = malloc(sz, tbl->llo_typ,
				       ((llo_flags & LLO_FLAGS_NOWAIT) ? M_NOWAIT : M_WAITOK));
		if (tbl->llo_hmtx == NULL) {
			goto out_err;
		}
		for(i=0; i<=tbl->llo_hashmod; i++) {
			LLO_MTX_INIT(tbl->llo_hmtx, i);
		}
	}	
	return(tbl);
out_err:
	if (tbl) {
		if (tbl->llo_epoch_start) {
			counter_u64_free(tbl->llo_epoch_start);
		}
		if (tbl->llo_epoch_end) {
			counter_u64_free(tbl->llo_epoch_end);
		}
		if (tbl->llo_ht) {
			hashdestroy(tbl->llo_ht, typ, tbl->llo_hashmod);
		}
		if (tbl->llo_hmtx) {
			free(tbl->llo_hmtx, typ);
		}
		free(tbl, typ);
	}
	return(NULL);
}

static void 
llo_do_destroy_table(struct llo_hash *llo)
{
	/* 
	 * All entries are properly gone at
	 * this point so we can purge the table.
	 */
	struct malloc_type *typ;

	/* Probably not needed. */
		
	callout_stop(&llo->lazy_clist_tmr);
	/* Now the purging */
	if (llo->llo_ht) {
		free(llo->llo_ht, llo->llo_typ);
		llo->llo_ht = NULL;
	}
	if (llo->llo_epoch_start) {
		counter_u64_free(llo->llo_epoch_start);
		llo->llo_epoch_start = NULL;
	}
	if (llo->llo_epoch_end) {
		counter_u64_free(llo->llo_epoch_end);
		llo->llo_epoch_end = NULL;
	}
	if (llo->llo_hmtx) {
		if (llo->table_flags & LLO_IFLAG_MMTX) {
			int i;
			for(i=0; i<= llo->llo_hashmod; i++) {
				LLO_MTX_DESTROY(llo->llo_hmtx, i);
			}
		} else {
			LLO_MTX_DESTROY(llo->llo_hmtx, 0);
		}
		llo->llo_hmtx = NULL;
	}
	LLO_MMTX_DESTROY(llo);
	typ = llo->llo_typ;
	free(llo, typ);
}

int 
llo_hash_destroy(struct llo_hash *llo)
{
	LLO_MMTX_LOCK(llo);
	if (llo->entries) {
		LLO_MMTX_UNLOCK(llo);
		return(EINVAL);
	}
	llo->table_flags |= LLO_IFLAG_PURGING;
	if (llo->being_deleted) {
		LLO_MMTX_UNLOCK(llo);
		return(0);
	}
	/* ok we can destroy it */
	llo_do_destroy_table(llo);
	return(0);
}

int 
llo_add_to_hash(struct llo_hash *llo, void *entry, void *key)
{
	struct llo_hash_head *bucket;
	struct llo_hash_entry *he, *ne;
	u_long hkey;
	uint32_t hidx;
	int mtx_idx;

	if (llo->table_flags & LLO_IFLAG_PURGING) {
		return (EINVAL);
	}
	/* Establish the bucket */
	hkey = (llo->llo_hf)(key);
	hidx = hkey % llo->llo_hashmod;
	bucket =  &llo->llo_ht[hidx];
	/* Now what type of lock? */
	if (llo->table_flags & LLO_IFLAG_MMTX) {
		mtx_idx = hidx;
	} else {
		mtx_idx = 0;
	}
	/* Get space for a new entry */
	ne = malloc(sizeof(struct llo_hash_entry), llo->llo_typ, 
		    ((llo->table_flags & LLO_IFLAG_NOWAIT) ? M_NOWAIT : M_WAITOK));
	if (ne == NULL) {
		/* No memory */
		return (-1);
	}
	ne->entry = entry;
	ne->key = key;
	ne->parent = llo;
	garbage_collect_init(&ne->gar);
	if (llo->table_flags & LLO_IFLAG_MINU64) {
		ne->delete_epoch = NULL;
	} else {
		ne->delete_epoch = counter_u64_alloc(((llo->table_flags & LLO_IFLAG_NOWAIT) ? M_NOWAIT : M_WAITOK));
		if (ne->delete_epoch == NULL) {
			free(ne, llo->llo_typ);
			return (-1);
		}
	}
	LLO_MTX_LOCK(llo->llo_hmtx, mtx_idx);
	/* Does it exist? */
  	LIST_FOREACH(he, bucket, next) {
		if((llo->llo_cf)(he->key, ne->key) == 0) {
			/* Already exists */
			LLO_MTX_UNLOCK(llo->llo_hmtx, mtx_idx);
			return(EEXIST);
		}
	}
	/* Ok lets add it */
	atomic_add_int(&llo->entries, 1);
	LIST_INSERT_HEAD(bucket, ne, next);
	LLO_MTX_UNLOCK(llo->llo_hmtx, mtx_idx);
	return (0);
}

void *
llo_hash_lookup(struct llo_hash *llo, void *key)
{
	struct llo_hash_head *bucket;
	struct llo_hash_entry *he;
	u_long hkey;
	uint32_t hidx;

	counter_u64_add(llo->llo_epoch_start, 1);
	hkey = (llo->llo_hf)(key);
	hidx = hkey % llo->llo_hashmod;
	bucket =  &llo->llo_ht[hidx];
  	LIST_FOREACH(he, bucket, next) {
		if((llo->llo_cf)(he->key, key) == 0) {
			return((void *)he);
		}
	}
	counter_u64_add(llo->llo_epoch_end, 1);
	return ((void *)NULL);
}

void 
llo_release(struct llo_hash *llo, void **entry, void *key)
{
	counter_u64_add(llo->llo_epoch_end, 1);
	*entry = NULL;
}

static void
llo_garfc(void *arg)
{
	struct llo_hash_entry *he;
	struct llo_hash *llo;
	struct timeval nxttm;
	void *e;
	
	he = (struct llo_hash_entry *)arg;
	llo = he->parent;
	/* Now we have all the pointers, can we delete? */
	if (counter_u64_is_gte(he->delete_epoch, llo->llo_epoch_end)) {
		/* Yes, lets do the delete's */
		e = he->entry;
		he->entry = NULL;
		he->key = NULL;
		he->parent = NULL;
		counter_u64_free(he->delete_epoch);
		free(he, llo->llo_typ);
		(llo->llo_ff)(e);
		atomic_subtract_int(&llo->being_deleted, 1);
		LLO_MMTX_LOCK(llo);
		if (llo->table_flags & LLO_IFLAG_PURGING) {
			/* This table is scheduled for deletion, can we yet? */
			if ((llo->being_deleted) || (llo->entries)) {
				/* 
				 * No, we check for entries too since there
				 * is a race that we ignore where an add happens
				 * at the same time as a destroy.
				 */
				LLO_MMTX_UNLOCK(llo);
				return;
			}
			/* Ok do the deed */
			llo_do_destroy_table(llo);
			return;
		}
		LLO_MMTX_UNLOCK(llo);
		return;
	}
	/* nope, we need to restart gc */
	garbage_collect_init(&he->gar);	
	nxttm.tv_sec = LLO_CALLOUT_SEC;
	nxttm.tv_usec =  LLO_CALLOUT_USEC;
	garbage_collect_add(&he->gar, llo_garfc, (void *)he, &nxttm, __FUNCTION__, __LINE__);
}

static void
llo_lazy_clist_to(void *arg)
{
	struct llo_hash *llo;
	struct llo_hash_entry *he;
	struct timeval nxttm;
	int need_lock;

	llo = (struct llo_hash *)arg;
	LLO_MMTX_LOCK(llo);
	if (callout_pending(&llo->lazy_clist_tmr)) {
		/* Callout has been rescheduled */
		LLO_MMTX_UNLOCK(llo);
		return;
	}
	if (!callout_active(&llo->lazy_clist_tmr)) {
		/* The callout has been stopped */
		LLO_MMTX_UNLOCK(llo);
		return;
	}
	callout_deactivate(&llo->lazy_clist_tmr);
	/* Now can we get any of our guys off the lazy_clist? */
	while ((he = LIST_FIRST(&llo->lazy_clist)) != NULL) {
		if ((llo->table_flags & LLO_IFLAG_NOWAIT) == 0) {
			/* We have to unlock */
			need_lock = 1;
			LLO_MMTX_UNLOCK(llo);
		} else {
			need_lock = 0;
		}
		he->delete_epoch = counter_u64_alloc(((llo->table_flags & LLO_IFLAG_NOWAIT) ? M_NOWAIT : M_WAITOK));
		if (need_lock) {
			LLO_MMTX_LOCK(llo);
		}
		if (he->delete_epoch == NULL) {		
			break;
		}
		LIST_REMOVE(he, cnext);
		counter_u64_copy(he->delete_epoch, llo->llo_epoch_start);
		/* Start the GC */
		nxttm.tv_sec = LLO_CALLOUT_SEC;
		nxttm.tv_usec =  LLO_CALLOUT_USEC;
		/* This won't fail unless the system is not init'd yet */
		garbage_collect_add(&he->gar, llo_garfc, (void *)he, &nxttm, __FUNCTION__, __LINE__);
	}
	/* Now do we need to start a new timer? */
	if (LIST_EMPTY(&llo->lazy_clist) == 0) {
		/* Yes */
		nxttm.tv_sec = LLO_CALLOUT_SEC;
		nxttm.tv_usec =  LLO_CALLOUT_USEC;
		callout_reset(&llo->lazy_clist_tmr,
			      (TV_TO_TICKS(&nxttm) + 1),
			      llo_lazy_clist_to, llo);
	} else {
		llo->table_flags &= ~LLO_IFLAG_CALLUP;
	}
	LLO_MMTX_UNLOCK(llo);
}

int 
llo_del_from_hash(struct llo_hash *llo, void *entry, void *key)
{
	struct llo_hash_head *bucket;
	struct llo_hash_entry *he;
	struct timeval nxttm;
	u_long hkey;
	uint32_t hidx;
	int mtx_idx;
	int retval=-1;
	int locked;
	/* Establish the bucket */
	hkey = (llo->llo_hf)(key);
	hidx = hkey % llo->llo_hashmod;
	bucket =  &llo->llo_ht[hidx];
	/* Now what type of lock? */
	if (llo->table_flags & LLO_IFLAG_MMTX) {
		mtx_idx = hidx;
	} else {
		mtx_idx = 0;
	}
	LLO_MTX_LOCK(llo->llo_hmtx, mtx_idx);
	locked = 1;
	/* Does it exist? */
  	LIST_FOREACH(he, bucket, next) {
		if((llo->llo_cf)(he->key, key) == 0) {
			/* Found it */
			LIST_REMOVE_STALE(he, next);
			atomic_add_int(&llo->being_deleted, 1);
			atomic_subtract_int(&llo->entries, 1);
			LLO_MTX_UNLOCK(llo->llo_hmtx, mtx_idx);
			retval = locked = 0;
			if (llo->table_flags & LLO_IFLAG_MINU64) {
				he->delete_epoch = counter_u64_alloc(((llo->table_flags & LLO_IFLAG_NOWAIT) ? M_NOWAIT : M_WAITOK));
				LLO_MMTX_LOCK(llo);
				if (he->delete_epoch == NULL) {
					/* We have an issue, no memory for out count.. postpone it */
					LIST_INSERT_HEAD(&llo->lazy_clist, he, cnext);
					if ((llo->table_flags & LLO_IFLAG_CALLUP) == 0) {
						/* Start a retry timer since nothing is up */
						nxttm.tv_sec = LLO_CALLOUT_SEC;
						nxttm.tv_usec =  LLO_CALLOUT_USEC;
						callout_reset(&llo->lazy_clist_tmr,
							      (TV_TO_TICKS(&nxttm) + 1),
							      llo_lazy_clist_to, llo);
						llo->table_flags |= LLO_IFLAG_CALLUP;
					}
					LLO_MMTX_UNLOCK(llo);
					return(0);
				}
				LLO_MMTX_UNLOCK(llo);
			}
			/* Now copy out the current start epoch to the delete epoch */
			counter_u64_copy(he->delete_epoch, llo->llo_epoch_start);
			/* Start the GC */
			nxttm.tv_sec = LLO_CALLOUT_SEC;
			nxttm.tv_usec =  LLO_CALLOUT_USEC;
			/* This won't fail unless the system is not init'd yet */
			garbage_collect_add(&he->gar, llo_garfc, (void *)he, &nxttm, __FUNCTION__, __LINE__);
			break;
		}
	}
	if (locked) {
		LLO_MTX_UNLOCK(llo->llo_hmtx, mtx_idx);
	}
	if (retval == -1) {
		retval = ENOENT;
	}
	return(retval);
}
