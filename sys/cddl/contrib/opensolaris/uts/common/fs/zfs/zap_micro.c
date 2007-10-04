/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/spa.h>
#include <sys/dmu.h>
#include <sys/zfs_context.h>
#include <sys/zap.h>
#include <sys/refcount.h>
#include <sys/zap_impl.h>
#include <sys/zap_leaf.h>
#include <sys/avl.h>


static void mzap_upgrade(zap_t *zap, dmu_tx_t *tx);


static void
mzap_byteswap(mzap_phys_t *buf, size_t size)
{
	int i, max;
	buf->mz_block_type = BSWAP_64(buf->mz_block_type);
	buf->mz_salt = BSWAP_64(buf->mz_salt);
	max = (size / MZAP_ENT_LEN) - 1;
	for (i = 0; i < max; i++) {
		buf->mz_chunk[i].mze_value =
		    BSWAP_64(buf->mz_chunk[i].mze_value);
		buf->mz_chunk[i].mze_cd =
		    BSWAP_32(buf->mz_chunk[i].mze_cd);
	}
}

void
zap_byteswap(void *buf, size_t size)
{
	uint64_t block_type;

	block_type = *(uint64_t *)buf;

	if (block_type == ZBT_MICRO || block_type == BSWAP_64(ZBT_MICRO)) {
		/* ASSERT(magic == ZAP_LEAF_MAGIC); */
		mzap_byteswap(buf, size);
	} else {
		fzap_byteswap(buf, size);
	}
}

static int
mze_compare(const void *arg1, const void *arg2)
{
	const mzap_ent_t *mze1 = arg1;
	const mzap_ent_t *mze2 = arg2;

	if (mze1->mze_hash > mze2->mze_hash)
		return (+1);
	if (mze1->mze_hash < mze2->mze_hash)
		return (-1);
	if (mze1->mze_phys.mze_cd > mze2->mze_phys.mze_cd)
		return (+1);
	if (mze1->mze_phys.mze_cd < mze2->mze_phys.mze_cd)
		return (-1);
	return (0);
}

static void
mze_insert(zap_t *zap, int chunkid, uint64_t hash, mzap_ent_phys_t *mzep)
{
	mzap_ent_t *mze;

	ASSERT(zap->zap_ismicro);
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));
	ASSERT(mzep->mze_cd < ZAP_MAXCD);
	ASSERT3U(zap_hash(zap, mzep->mze_name), ==, hash);

	mze = kmem_alloc(sizeof (mzap_ent_t), KM_SLEEP);
	mze->mze_chunkid = chunkid;
	mze->mze_hash = hash;
	mze->mze_phys = *mzep;
	avl_add(&zap->zap_m.zap_avl, mze);
}

static mzap_ent_t *
mze_find(zap_t *zap, const char *name, uint64_t hash)
{
	mzap_ent_t mze_tofind;
	mzap_ent_t *mze;
	avl_index_t idx;
	avl_tree_t *avl = &zap->zap_m.zap_avl;

	ASSERT(zap->zap_ismicro);
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	ASSERT3U(zap_hash(zap, name), ==, hash);

	if (strlen(name) >= sizeof (mze_tofind.mze_phys.mze_name))
		return (NULL);

	mze_tofind.mze_hash = hash;
	mze_tofind.mze_phys.mze_cd = 0;

	mze = avl_find(avl, &mze_tofind, &idx);
	if (mze == NULL)
		mze = avl_nearest(avl, idx, AVL_AFTER);
	for (; mze && mze->mze_hash == hash; mze = AVL_NEXT(avl, mze)) {
		if (strcmp(name, mze->mze_phys.mze_name) == 0)
			return (mze);
	}
	return (NULL);
}

static uint32_t
mze_find_unused_cd(zap_t *zap, uint64_t hash)
{
	mzap_ent_t mze_tofind;
	mzap_ent_t *mze;
	avl_index_t idx;
	avl_tree_t *avl = &zap->zap_m.zap_avl;
	uint32_t cd;

	ASSERT(zap->zap_ismicro);
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));

	mze_tofind.mze_hash = hash;
	mze_tofind.mze_phys.mze_cd = 0;

	cd = 0;
	for (mze = avl_find(avl, &mze_tofind, &idx);
	    mze && mze->mze_hash == hash; mze = AVL_NEXT(avl, mze)) {
		if (mze->mze_phys.mze_cd != cd)
			break;
		cd++;
	}

	return (cd);
}

static void
mze_remove(zap_t *zap, mzap_ent_t *mze)
{
	ASSERT(zap->zap_ismicro);
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

	avl_remove(&zap->zap_m.zap_avl, mze);
	kmem_free(mze, sizeof (mzap_ent_t));
}

static void
mze_destroy(zap_t *zap)
{
	mzap_ent_t *mze;
	void *avlcookie = NULL;

	while (mze = avl_destroy_nodes(&zap->zap_m.zap_avl, &avlcookie))
		kmem_free(mze, sizeof (mzap_ent_t));
	avl_destroy(&zap->zap_m.zap_avl);
}

static zap_t *
mzap_open(objset_t *os, uint64_t obj, dmu_buf_t *db)
{
	zap_t *winner;
	zap_t *zap;
	int i;

	ASSERT3U(MZAP_ENT_LEN, ==, sizeof (mzap_ent_phys_t));

	zap = kmem_zalloc(sizeof (zap_t), KM_SLEEP);
	rw_init(&zap->zap_rwlock, NULL, RW_DEFAULT, 0);
	rw_enter(&zap->zap_rwlock, RW_WRITER);
	zap->zap_objset = os;
	zap->zap_object = obj;
	zap->zap_dbuf = db;

	if (((uint64_t *)db->db_data)[0] != ZBT_MICRO) {
		mutex_init(&zap->zap_f.zap_num_entries_mtx, NULL,
		    MUTEX_DEFAULT, 0);
		zap->zap_f.zap_block_shift = highbit(db->db_size) - 1;
	} else {
		zap->zap_ismicro = TRUE;
	}

	/*
	 * Make sure that zap_ismicro is set before we let others see
	 * it, because zap_lockdir() checks zap_ismicro without the lock
	 * held.
	 */
	winner = dmu_buf_set_user(db, zap, &zap->zap_m.zap_phys, zap_evict);

	if (winner != NULL) {
		rw_exit(&zap->zap_rwlock);
		rw_destroy(&zap->zap_rwlock);
		if (!zap->zap_ismicro)
			mutex_destroy(&zap->zap_f.zap_num_entries_mtx);
		kmem_free(zap, sizeof (zap_t));
		return (winner);
	}

	if (zap->zap_ismicro) {
		zap->zap_salt = zap->zap_m.zap_phys->mz_salt;
		zap->zap_m.zap_num_chunks = db->db_size / MZAP_ENT_LEN - 1;
		avl_create(&zap->zap_m.zap_avl, mze_compare,
		    sizeof (mzap_ent_t), offsetof(mzap_ent_t, mze_node));

		for (i = 0; i < zap->zap_m.zap_num_chunks; i++) {
			mzap_ent_phys_t *mze =
			    &zap->zap_m.zap_phys->mz_chunk[i];
			if (mze->mze_name[0]) {
				zap->zap_m.zap_num_entries++;
				mze_insert(zap, i,
				    zap_hash(zap, mze->mze_name), mze);
			}
		}
	} else {
		zap->zap_salt = zap->zap_f.zap_phys->zap_salt;

		ASSERT3U(sizeof (struct zap_leaf_header), ==,
		    2*ZAP_LEAF_CHUNKSIZE);

		/*
		 * The embedded pointer table should not overlap the
		 * other members.
		 */
		ASSERT3P(&ZAP_EMBEDDED_PTRTBL_ENT(zap, 0), >,
		    &zap->zap_f.zap_phys->zap_salt);

		/*
		 * The embedded pointer table should end at the end of
		 * the block
		 */
		ASSERT3U((uintptr_t)&ZAP_EMBEDDED_PTRTBL_ENT(zap,
		    1<<ZAP_EMBEDDED_PTRTBL_SHIFT(zap)) -
		    (uintptr_t)zap->zap_f.zap_phys, ==,
		    zap->zap_dbuf->db_size);
	}
	rw_exit(&zap->zap_rwlock);
	return (zap);
}

int
zap_lockdir(objset_t *os, uint64_t obj, dmu_tx_t *tx,
    krw_t lti, int fatreader, zap_t **zapp)
{
	zap_t *zap;
	dmu_buf_t *db;
	krw_t lt;
	int err;

	*zapp = NULL;

	err = dmu_buf_hold(os, obj, 0, NULL, &db);
	if (err)
		return (err);

#ifdef ZFS_DEBUG
	{
		dmu_object_info_t doi;
		dmu_object_info_from_db(db, &doi);
		ASSERT(dmu_ot[doi.doi_type].ot_byteswap == zap_byteswap);
	}
#endif

	zap = dmu_buf_get_user(db);
	if (zap == NULL)
		zap = mzap_open(os, obj, db);

	/*
	 * We're checking zap_ismicro without the lock held, in order to
	 * tell what type of lock we want.  Once we have some sort of
	 * lock, see if it really is the right type.  In practice this
	 * can only be different if it was upgraded from micro to fat,
	 * and micro wanted WRITER but fat only needs READER.
	 */
	lt = (!zap->zap_ismicro && fatreader) ? RW_READER : lti;
	rw_enter(&zap->zap_rwlock, lt);
	if (lt != ((!zap->zap_ismicro && fatreader) ? RW_READER : lti)) {
		/* it was upgraded, now we only need reader */
		ASSERT(lt == RW_WRITER);
		ASSERT(RW_READER ==
		    (!zap->zap_ismicro && fatreader) ? RW_READER : lti);
		rw_downgrade(&zap->zap_rwlock);
		lt = RW_READER;
	}

	zap->zap_objset = os;

	if (lt == RW_WRITER)
		dmu_buf_will_dirty(db, tx);

	ASSERT3P(zap->zap_dbuf, ==, db);

	ASSERT(!zap->zap_ismicro ||
	    zap->zap_m.zap_num_entries <= zap->zap_m.zap_num_chunks);
	if (zap->zap_ismicro && tx &&
	    zap->zap_m.zap_num_entries == zap->zap_m.zap_num_chunks) {
		uint64_t newsz = db->db_size + SPA_MINBLOCKSIZE;
		if (newsz > MZAP_MAX_BLKSZ) {
			dprintf("upgrading obj %llu: num_entries=%u\n",
			    obj, zap->zap_m.zap_num_entries);
			mzap_upgrade(zap, tx);
			*zapp = zap;
			return (0);
		}
		err = dmu_object_set_blocksize(os, obj, newsz, 0, tx);
		ASSERT3U(err, ==, 0);
		zap->zap_m.zap_num_chunks =
		    db->db_size / MZAP_ENT_LEN - 1;
	}

	*zapp = zap;
	return (0);
}

void
zap_unlockdir(zap_t *zap)
{
	rw_exit(&zap->zap_rwlock);
	dmu_buf_rele(zap->zap_dbuf, NULL);
}

static void
mzap_upgrade(zap_t *zap, dmu_tx_t *tx)
{
	mzap_phys_t *mzp;
	int i, sz, nchunks, err;

	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

	sz = zap->zap_dbuf->db_size;
	mzp = kmem_alloc(sz, KM_SLEEP);
	bcopy(zap->zap_dbuf->db_data, mzp, sz);
	nchunks = zap->zap_m.zap_num_chunks;

	err = dmu_object_set_blocksize(zap->zap_objset, zap->zap_object,
	    1ULL << fzap_default_block_shift, 0, tx);
	ASSERT(err == 0);

	dprintf("upgrading obj=%llu with %u chunks\n",
	    zap->zap_object, nchunks);
	mze_destroy(zap);

	fzap_upgrade(zap, tx);

	for (i = 0; i < nchunks; i++) {
		int err;
		mzap_ent_phys_t *mze = &mzp->mz_chunk[i];
		if (mze->mze_name[0] == 0)
			continue;
		dprintf("adding %s=%llu\n",
		    mze->mze_name, mze->mze_value);
		err = fzap_add_cd(zap,
		    mze->mze_name, 8, 1, &mze->mze_value,
		    mze->mze_cd, tx);
		ASSERT3U(err, ==, 0);
	}
	kmem_free(mzp, sz);
}

uint64_t
zap_hash(zap_t *zap, const char *name)
{
	const uint8_t *cp;
	uint8_t c;
	uint64_t crc = zap->zap_salt;

	ASSERT(crc != 0);
	ASSERT(zfs_crc64_table[128] == ZFS_CRC64_POLY);
	for (cp = (const uint8_t *)name; (c = *cp) != '\0'; cp++)
		crc = (crc >> 8) ^ zfs_crc64_table[(crc ^ c) & 0xFF];

	/*
	 * Only use 28 bits, since we need 4 bits in the cookie for the
	 * collision differentiator.  We MUST use the high bits, since
	 * those are the onces that we first pay attention to when
	 * chosing the bucket.
	 */
	crc &= ~((1ULL << (64 - ZAP_HASHBITS)) - 1);

	return (crc);
}


static void
mzap_create_impl(objset_t *os, uint64_t obj, dmu_tx_t *tx)
{
	dmu_buf_t *db;
	mzap_phys_t *zp;

	VERIFY(0 == dmu_buf_hold(os, obj, 0, FTAG, &db));

#ifdef ZFS_DEBUG
	{
		dmu_object_info_t doi;
		dmu_object_info_from_db(db, &doi);
		ASSERT(dmu_ot[doi.doi_type].ot_byteswap == zap_byteswap);
	}
#endif

	dmu_buf_will_dirty(db, tx);
	zp = db->db_data;
	zp->mz_block_type = ZBT_MICRO;
	zp->mz_salt = ((uintptr_t)db ^ (uintptr_t)tx ^ (obj << 1)) | 1ULL;
	ASSERT(zp->mz_salt != 0);
	dmu_buf_rele(db, FTAG);
}

int
zap_create_claim(objset_t *os, uint64_t obj, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx)
{
	int err;

	err = dmu_object_claim(os, obj, ot, 0, bonustype, bonuslen, tx);
	if (err != 0)
		return (err);
	mzap_create_impl(os, obj, tx);
	return (0);
}

uint64_t
zap_create(objset_t *os, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx)
{
	uint64_t obj = dmu_object_alloc(os, ot, 0, bonustype, bonuslen, tx);

	mzap_create_impl(os, obj, tx);
	return (obj);
}

int
zap_destroy(objset_t *os, uint64_t zapobj, dmu_tx_t *tx)
{
	/*
	 * dmu_object_free will free the object number and free the
	 * data.  Freeing the data will cause our pageout function to be
	 * called, which will destroy our data (zap_leaf_t's and zap_t).
	 */

	return (dmu_object_free(os, zapobj, tx));
}

_NOTE(ARGSUSED(0))
void
zap_evict(dmu_buf_t *db, void *vzap)
{
	zap_t *zap = vzap;

	rw_destroy(&zap->zap_rwlock);

	if (zap->zap_ismicro)
		mze_destroy(zap);
	else
		mutex_destroy(&zap->zap_f.zap_num_entries_mtx);

	kmem_free(zap, sizeof (zap_t));
}

int
zap_count(objset_t *os, uint64_t zapobj, uint64_t *count)
{
	zap_t *zap;
	int err;

	err = zap_lockdir(os, zapobj, NULL, RW_READER, TRUE, &zap);
	if (err)
		return (err);
	if (!zap->zap_ismicro) {
		err = fzap_count(zap, count);
	} else {
		*count = zap->zap_m.zap_num_entries;
	}
	zap_unlockdir(zap);
	return (err);
}

/*
 * Routines for maniplulating attributes.
 */

int
zap_lookup(objset_t *os, uint64_t zapobj, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf)
{
	zap_t *zap;
	int err;
	mzap_ent_t *mze;

	err = zap_lockdir(os, zapobj, NULL, RW_READER, TRUE, &zap);
	if (err)
		return (err);
	if (!zap->zap_ismicro) {
		err = fzap_lookup(zap, name,
		    integer_size, num_integers, buf);
	} else {
		mze = mze_find(zap, name, zap_hash(zap, name));
		if (mze == NULL) {
			err = ENOENT;
		} else {
			if (num_integers < 1)
				err = EOVERFLOW;
			else if (integer_size != 8)
				err = EINVAL;
			else
				*(uint64_t *)buf = mze->mze_phys.mze_value;
		}
	}
	zap_unlockdir(zap);
	return (err);
}

int
zap_length(objset_t *os, uint64_t zapobj, const char *name,
    uint64_t *integer_size, uint64_t *num_integers)
{
	zap_t *zap;
	int err;
	mzap_ent_t *mze;

	err = zap_lockdir(os, zapobj, NULL, RW_READER, TRUE, &zap);
	if (err)
		return (err);
	if (!zap->zap_ismicro) {
		err = fzap_length(zap, name, integer_size, num_integers);
	} else {
		mze = mze_find(zap, name, zap_hash(zap, name));
		if (mze == NULL) {
			err = ENOENT;
		} else {
			if (integer_size)
				*integer_size = 8;
			if (num_integers)
				*num_integers = 1;
		}
	}
	zap_unlockdir(zap);
	return (err);
}

static void
mzap_addent(zap_t *zap, const char *name, uint64_t hash, uint64_t value)
{
	int i;
	int start = zap->zap_m.zap_alloc_next;
	uint32_t cd;

	dprintf("obj=%llu %s=%llu\n", zap->zap_object, name, value);
	ASSERT(RW_WRITE_HELD(&zap->zap_rwlock));

#ifdef ZFS_DEBUG
	for (i = 0; i < zap->zap_m.zap_num_chunks; i++) {
		mzap_ent_phys_t *mze = &zap->zap_m.zap_phys->mz_chunk[i];
		ASSERT(strcmp(name, mze->mze_name) != 0);
	}
#endif

	cd = mze_find_unused_cd(zap, hash);
	/* given the limited size of the microzap, this can't happen */
	ASSERT(cd != ZAP_MAXCD);

again:
	for (i = start; i < zap->zap_m.zap_num_chunks; i++) {
		mzap_ent_phys_t *mze = &zap->zap_m.zap_phys->mz_chunk[i];
		if (mze->mze_name[0] == 0) {
			mze->mze_value = value;
			mze->mze_cd = cd;
			(void) strcpy(mze->mze_name, name);
			zap->zap_m.zap_num_entries++;
			zap->zap_m.zap_alloc_next = i+1;
			if (zap->zap_m.zap_alloc_next ==
			    zap->zap_m.zap_num_chunks)
				zap->zap_m.zap_alloc_next = 0;
			mze_insert(zap, i, hash, mze);
			return;
		}
	}
	if (start != 0) {
		start = 0;
		goto again;
	}
	ASSERT(!"out of entries!");
}

int
zap_add(objset_t *os, uint64_t zapobj, const char *name,
    int integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx)
{
	zap_t *zap;
	int err;
	mzap_ent_t *mze;
	const uint64_t *intval = val;
	uint64_t hash;

	err = zap_lockdir(os, zapobj, tx, RW_WRITER, TRUE, &zap);
	if (err)
		return (err);
	if (!zap->zap_ismicro) {
		err = fzap_add(zap, name, integer_size, num_integers, val, tx);
	} else if (integer_size != 8 || num_integers != 1 ||
	    strlen(name) >= MZAP_NAME_LEN) {
		dprintf("upgrading obj %llu: intsz=%u numint=%llu name=%s\n",
		    zapobj, integer_size, num_integers, name);
		mzap_upgrade(zap, tx);
		err = fzap_add(zap, name, integer_size, num_integers, val, tx);
	} else {
		hash = zap_hash(zap, name);
		mze = mze_find(zap, name, hash);
		if (mze != NULL) {
			err = EEXIST;
		} else {
			mzap_addent(zap, name, hash, *intval);
		}
	}
	zap_unlockdir(zap);
	return (err);
}

int
zap_update(objset_t *os, uint64_t zapobj, const char *name,
    int integer_size, uint64_t num_integers, const void *val, dmu_tx_t *tx)
{
	zap_t *zap;
	mzap_ent_t *mze;
	const uint64_t *intval = val;
	uint64_t hash;
	int err;

	err = zap_lockdir(os, zapobj, tx, RW_WRITER, TRUE, &zap);
	if (err)
		return (err);
	ASSERT(RW_LOCK_HELD(&zap->zap_rwlock));
	if (!zap->zap_ismicro) {
		err = fzap_update(zap, name,
		    integer_size, num_integers, val, tx);
	} else if (integer_size != 8 || num_integers != 1 ||
	    strlen(name) >= MZAP_NAME_LEN) {
		dprintf("upgrading obj %llu: intsz=%u numint=%llu name=%s\n",
		    zapobj, integer_size, num_integers, name);
		mzap_upgrade(zap, tx);
		err = fzap_update(zap, name,
		    integer_size, num_integers, val, tx);
	} else {
		hash = zap_hash(zap, name);
		mze = mze_find(zap, name, hash);
		if (mze != NULL) {
			mze->mze_phys.mze_value = *intval;
			zap->zap_m.zap_phys->mz_chunk
			    [mze->mze_chunkid].mze_value = *intval;
		} else {
			mzap_addent(zap, name, hash, *intval);
		}
	}
	zap_unlockdir(zap);
	return (err);
}

int
zap_remove(objset_t *os, uint64_t zapobj, const char *name, dmu_tx_t *tx)
{
	zap_t *zap;
	int err;
	mzap_ent_t *mze;

	err = zap_lockdir(os, zapobj, tx, RW_WRITER, TRUE, &zap);
	if (err)
		return (err);
	if (!zap->zap_ismicro) {
		err = fzap_remove(zap, name, tx);
	} else {
		mze = mze_find(zap, name, zap_hash(zap, name));
		if (mze == NULL) {
			dprintf("fail: %s\n", name);
			err = ENOENT;
		} else {
			dprintf("success: %s\n", name);
			zap->zap_m.zap_num_entries--;
			bzero(&zap->zap_m.zap_phys->mz_chunk[mze->mze_chunkid],
			    sizeof (mzap_ent_phys_t));
			mze_remove(zap, mze);
		}
	}
	zap_unlockdir(zap);
	return (err);
}


/*
 * Routines for iterating over the attributes.
 */

/*
 * We want to keep the high 32 bits of the cursor zero if we can, so
 * that 32-bit programs can access this.  So use a small hash value so
 * we can fit 4 bits of cd into the 32-bit cursor.
 *
 * [ 4 zero bits | 32-bit collision differentiator | 28-bit hash value ]
 */
void
zap_cursor_init_serialized(zap_cursor_t *zc, objset_t *os, uint64_t zapobj,
    uint64_t serialized)
{
	zc->zc_objset = os;
	zc->zc_zap = NULL;
	zc->zc_leaf = NULL;
	zc->zc_zapobj = zapobj;
	if (serialized == -1ULL) {
		zc->zc_hash = -1ULL;
		zc->zc_cd = 0;
	} else {
		zc->zc_hash = serialized << (64-ZAP_HASHBITS);
		zc->zc_cd = serialized >> ZAP_HASHBITS;
		if (zc->zc_cd >= ZAP_MAXCD) /* corrupt serialized */
			zc->zc_cd = 0;
	}
}

void
zap_cursor_init(zap_cursor_t *zc, objset_t *os, uint64_t zapobj)
{
	zap_cursor_init_serialized(zc, os, zapobj, 0);
}

void
zap_cursor_fini(zap_cursor_t *zc)
{
	if (zc->zc_zap) {
		rw_enter(&zc->zc_zap->zap_rwlock, RW_READER);
		zap_unlockdir(zc->zc_zap);
		zc->zc_zap = NULL;
	}
	if (zc->zc_leaf) {
		rw_enter(&zc->zc_leaf->l_rwlock, RW_READER);
		zap_put_leaf(zc->zc_leaf);
		zc->zc_leaf = NULL;
	}
	zc->zc_objset = NULL;
}

uint64_t
zap_cursor_serialize(zap_cursor_t *zc)
{
	if (zc->zc_hash == -1ULL)
		return (-1ULL);
	ASSERT((zc->zc_hash & (ZAP_MAXCD-1)) == 0);
	ASSERT(zc->zc_cd < ZAP_MAXCD);
	return ((zc->zc_hash >> (64-ZAP_HASHBITS)) |
	    ((uint64_t)zc->zc_cd << ZAP_HASHBITS));
}

int
zap_cursor_retrieve(zap_cursor_t *zc, zap_attribute_t *za)
{
	int err;
	avl_index_t idx;
	mzap_ent_t mze_tofind;
	mzap_ent_t *mze;

	if (zc->zc_hash == -1ULL)
		return (ENOENT);

	if (zc->zc_zap == NULL) {
		err = zap_lockdir(zc->zc_objset, zc->zc_zapobj, NULL,
		    RW_READER, TRUE, &zc->zc_zap);
		if (err)
			return (err);
	} else {
		rw_enter(&zc->zc_zap->zap_rwlock, RW_READER);
	}
	if (!zc->zc_zap->zap_ismicro) {
		err = fzap_cursor_retrieve(zc->zc_zap, zc, za);
	} else {
		err = ENOENT;

		mze_tofind.mze_hash = zc->zc_hash;
		mze_tofind.mze_phys.mze_cd = zc->zc_cd;

		mze = avl_find(&zc->zc_zap->zap_m.zap_avl, &mze_tofind, &idx);
		ASSERT(mze == NULL || 0 == bcmp(&mze->mze_phys,
		    &zc->zc_zap->zap_m.zap_phys->mz_chunk[mze->mze_chunkid],
		    sizeof (mze->mze_phys)));
		if (mze == NULL) {
			mze = avl_nearest(&zc->zc_zap->zap_m.zap_avl,
			    idx, AVL_AFTER);
		}
		if (mze) {
			za->za_integer_length = 8;
			za->za_num_integers = 1;
			za->za_first_integer = mze->mze_phys.mze_value;
			(void) strcpy(za->za_name, mze->mze_phys.mze_name);
			zc->zc_hash = mze->mze_hash;
			zc->zc_cd = mze->mze_phys.mze_cd;
			err = 0;
		} else {
			zc->zc_hash = -1ULL;
		}
	}
	rw_exit(&zc->zc_zap->zap_rwlock);
	return (err);
}

void
zap_cursor_advance(zap_cursor_t *zc)
{
	if (zc->zc_hash == -1ULL)
		return;
	zc->zc_cd++;
	if (zc->zc_cd >= ZAP_MAXCD) {
		zc->zc_cd = 0;
		zc->zc_hash += 1ULL<<(64-ZAP_HASHBITS);
		if (zc->zc_hash == 0) /* EOF */
			zc->zc_hash = -1ULL;
	}
}

int
zap_get_stats(objset_t *os, uint64_t zapobj, zap_stats_t *zs)
{
	int err;
	zap_t *zap;

	err = zap_lockdir(os, zapobj, NULL, RW_READER, TRUE, &zap);
	if (err)
		return (err);

	bzero(zs, sizeof (zap_stats_t));

	if (zap->zap_ismicro) {
		zs->zs_blocksize = zap->zap_dbuf->db_size;
		zs->zs_num_entries = zap->zap_m.zap_num_entries;
		zs->zs_num_blocks = 1;
	} else {
		fzap_get_stats(zap, zs);
	}
	zap_unlockdir(zap);
	return (0);
}
