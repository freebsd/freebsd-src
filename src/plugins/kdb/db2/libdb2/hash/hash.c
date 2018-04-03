/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash.c	8.12 (Berkeley) 11/7/95";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef DEBUG
#include <assert.h>
#endif

#include "db-int.h"
#include "hash.h"
#include "page.h"
#include "extern.h"

static int32_t flush_meta __P((HTAB *));
static int32_t hash_access __P((HTAB *, ACTION, const DBT *, DBT *));
static int32_t hash_close __P((DB *));
static int32_t hash_delete __P((const DB *, const DBT *, u_int32_t));
static int32_t hash_fd __P((const DB *));
static int32_t hash_get __P((const DB *, const DBT *, DBT *, u_int32_t));
static int32_t hash_put __P((const DB *, DBT *, const DBT *, u_int32_t));
static int32_t hash_seq __P((const DB *, DBT *, DBT *, u_int32_t));
static int32_t hash_sync __P((const DB *, u_int32_t));
static int32_t hdestroy __P((HTAB *));
static int32_t cursor_get __P((const DB *, CURSOR *, DBT *, DBT *, \
	u_int32_t));
static int32_t cursor_delete __P((const DB *, CURSOR *, u_int32_t));
static HTAB *init_hash __P((HTAB *, const char *, const HASHINFO *));
static int32_t init_htab __P((HTAB *, int32_t));
#if DB_BYTE_ORDER == DB_LITTLE_ENDIAN
static void swap_header __P((HTAB *));
static void swap_header_copy __P((HASHHDR *, HASHHDR *));
#endif
static u_int32_t hget_header __P((HTAB *, u_int32_t));
static void hput_header __P((HTAB *));

#define RETURN_ERROR(ERR, LOC)	{ save_errno = ERR; goto LOC; }

/* Return values */
#define	SUCCESS	 (0)
#define	ERROR	(-1)
#define	ABNORMAL (1)

#ifdef HASH_STATISTICS
u_int32_t hash_accesses, hash_collisions, hash_expansions, hash_overflows,
	hash_bigpages;
#endif

/************************** INTERFACE ROUTINES ***************************/
/* OPEN/CLOSE */

extern DB *
__kdb2_hash_open(file, flags, mode, info, dflags)
	const char *file;
	int flags, mode, dflags;
	const HASHINFO *info;	/* Special directives for create */
{
	struct stat statbuf;
	DB *dbp;
	DBT mpool_key;
	HTAB *hashp;
	int32_t bpages, csize, new_table, save_errno;

	if (!file || (flags & O_ACCMODE) == O_WRONLY) {
		errno = EINVAL;
		return (NULL);
	}
	if (!(hashp = (HTAB *)calloc(1, sizeof(HTAB))))
		return (NULL);
	hashp->fp = -1;
	/*
	 * Even if user wants write only, we need to be able to read
	 * the actual file, so we need to open it read/write. But, the
	 * field in the hashp structure needs to be accurate so that
	 * we can check accesses.
	 */
	hashp->flags = flags;
	hashp->save_file = hashp->flags & O_RDWR;

	new_table = 0;
	if (!file || (flags & O_TRUNC) ||
	    (stat(file, &statbuf) && (errno == ENOENT))) {
		if (errno == ENOENT)
			errno = 0;	/* In case someone looks at errno. */
		new_table = 1;
	}
	if (file) {
		if ((hashp->fp = open(file, flags|O_BINARY, mode)) == -1)
			RETURN_ERROR(errno, error0);
		(void)fcntl(hashp->fp, F_SETFD, 1);
	}

	/* Process arguments to set up hash table header. */
	if (new_table) {
		if (!(hashp = init_hash(hashp, file, info)))
			RETURN_ERROR(errno, error1);
	} else {
		/* Table already exists */
		if (info && info->hash)
			hashp->hash = info->hash;
		else
			hashp->hash = __default_hash;

		/* copy metadata from page into header */
		if (hget_header(hashp,
		    (info && info->bsize ? info->bsize : DEF_BUCKET_SIZE)) !=
		    sizeof(HASHHDR))
			RETURN_ERROR(EFTYPE, error1);

		/* Verify file type, versions and hash function */
		if (hashp->hdr.magic != HASHMAGIC)
			RETURN_ERROR(EFTYPE, error1);
#define	OLDHASHVERSION	1
		if (hashp->hdr.version != HASHVERSION &&
		    hashp->hdr.version != OLDHASHVERSION)
			RETURN_ERROR(EFTYPE, error1);
		if (hashp->hash(CHARKEY, sizeof(CHARKEY))
		    != hashp->hdr.h_charkey)
			RETURN_ERROR(EFTYPE, error1);
		/*
		 * Figure out how many segments we need.  Max_Bucket is the
		 * maximum bucket number, so the number of buckets is
		 * max_bucket + 1.
		 */

		/* Read in bitmaps */
		bpages = (hashp->hdr.spares[hashp->hdr.ovfl_point] +
		    (hashp->hdr.bsize << BYTE_SHIFT) - 1) >>
		    (hashp->hdr.bshift + BYTE_SHIFT);

		hashp->nmaps = bpages;
		(void)memset(&hashp->mapp[0], 0, bpages * sizeof(u_int32_t *));
	}

	/* start up mpool */
	mpool_key.data = (u_int8_t *)file;
	mpool_key.size = strlen(file);

	if (info && info->cachesize)
		csize = info->cachesize / hashp->hdr.bsize;
	else
		csize = DEF_CACHESIZE / hashp->hdr.bsize;
	hashp->mp = mpool_open(&mpool_key, hashp->fp, hashp->hdr.bsize, csize);

	if (!hashp->mp)
		RETURN_ERROR(errno, error1);
	mpool_filter(hashp->mp, __pgin_routine, __pgout_routine, hashp);

	/*
	 * For a new table, set up the bitmaps.
	 */
	if (new_table &&
	   init_htab(hashp, info && info->nelem ? info->nelem : 1))
		goto error2;

	/* initialize the cursor queue */
	TAILQ_INIT(&hashp->curs_queue);
	hashp->seq_cursor = NULL;


	/* get a chunk of memory for our split buffer */
	hashp->split_buf = (PAGE16 *)malloc(hashp->hdr.bsize);
	if (!hashp->split_buf)
		goto error2;

	hashp->new_file = new_table;

	if (!(dbp = (DB *)malloc(sizeof(DB))))
		goto error2;

	dbp->internal = hashp;
	dbp->close = hash_close;
	dbp->del = hash_delete;
	dbp->fd = hash_fd;
	dbp->get = hash_get;
	dbp->put = hash_put;
	dbp->seq = hash_seq;
	dbp->sync = hash_sync;
	dbp->type = DB_HASH;

#ifdef DEBUG
	(void)fprintf(stderr,
	    "%s\n%s%lx\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%x\n%s%x\n%s%d\n%s%d\n",
	    "init_htab:",
	    "TABLE POINTER   ", (void *)hashp,
	    "BUCKET SIZE     ", hashp->hdr.bsize,
	    "BUCKET SHIFT    ", hashp->hdr.bshift,
	    "FILL FACTOR     ", hashp->hdr.ffactor,
	    "MAX BUCKET      ", hashp->hdr.max_bucket,
	    "OVFL POINT      ", hashp->hdr.ovfl_point,
	    "LAST FREED      ", hashp->hdr.last_freed,
	    "HIGH MASK       ", hashp->hdr.high_mask,
	    "LOW  MASK       ", hashp->hdr.low_mask,
	    "NKEYS           ", hashp->hdr.nkeys);
#endif
#ifdef HASH_STATISTICS
	hash_overflows = hash_accesses = hash_collisions = hash_expansions = 0;
	hash_bigpages = 0;
#endif
	return (dbp);

error2:
	save_errno = errno;
	hdestroy(hashp);
	errno = save_errno;
	return (NULL);

error1:
	if (hashp != NULL)
		(void)close(hashp->fp);

error0:
	free(hashp);
	errno = save_errno;
	return (NULL);
}

static int32_t
hash_close(dbp)
	DB *dbp;
{
	HTAB *hashp;
	int32_t retval;

	if (!dbp)
		return (ERROR);

	hashp = (HTAB *)dbp->internal;
	retval = hdestroy(hashp);
	free(dbp);
	return (retval);
}

static int32_t
hash_fd(dbp)
	const DB *dbp;
{
	HTAB *hashp;

	if (!dbp)
		return (ERROR);

	hashp = (HTAB *)dbp->internal;
	if (hashp->fp == -1) {
		errno = ENOENT;
		return (-1);
	}
	return (hashp->fp);
}

/************************** LOCAL CREATION ROUTINES **********************/
static HTAB *
init_hash(hashp, file, info)
	HTAB *hashp;
	const char *file;
	const HASHINFO *info;
{
	struct stat statbuf;

	hashp->hdr.nkeys = 0;
	hashp->hdr.lorder = DB_BYTE_ORDER;
	hashp->hdr.bsize = DEF_BUCKET_SIZE;
	hashp->hdr.bshift = DEF_BUCKET_SHIFT;
	hashp->hdr.ffactor = DEF_FFACTOR;
	hashp->hash = __default_hash;
	memset(hashp->hdr.spares, 0, sizeof(hashp->hdr.spares));
	memset(hashp->hdr.bitmaps, 0, sizeof(hashp->hdr.bitmaps));

	/* Fix bucket size to be optimal for file system */
	if (file != NULL) {
		if (stat(file, &statbuf))
			return (NULL);
		hashp->hdr.bsize = statbuf.st_blksize;
		if (hashp->hdr.bsize > MAX_BSIZE)
		    hashp->hdr.bsize = MAX_BSIZE;
		hashp->hdr.bshift = __log2(hashp->hdr.bsize);
	}
	if (info) {
		if (info->bsize) {
			/* Round pagesize up to power of 2 */
			hashp->hdr.bshift = __log2(info->bsize);
			hashp->hdr.bsize = 1 << hashp->hdr.bshift;
			if (hashp->hdr.bsize > MAX_BSIZE) {
				errno = EINVAL;
				return (NULL);
			}
		}
		if (info->ffactor)
			hashp->hdr.ffactor = info->ffactor;
		if (info->hash)
			hashp->hash = info->hash;
		if (info->lorder) {
			if ((info->lorder != DB_BIG_ENDIAN) &&
			    (info->lorder != DB_LITTLE_ENDIAN)) {
				errno = EINVAL;
				return (NULL);
			}
			hashp->hdr.lorder = info->lorder;
		}
	}
	return (hashp);
}

/*
 * Returns 0 on No Error
 */
static int32_t
init_htab(hashp, nelem)
	HTAB *hashp;
	int32_t nelem;
{
	int32_t l2, nbuckets;

	/*
	 * Divide number of elements by the fill factor and determine a
	 * desired number of buckets.  Allocate space for the next greater
	 * power of two number of buckets.
	 */
	nelem = (nelem - 1) / hashp->hdr.ffactor + 1;

	l2 = __log2(MAX(nelem, 2));
	nbuckets = 1 << l2;

	hashp->hdr.spares[l2] = l2 + 1;
	hashp->hdr.spares[l2 + 1] = l2 + 1;
	hashp->hdr.ovfl_point = l2;
	hashp->hdr.last_freed = 2;

	hashp->hdr.max_bucket = hashp->hdr.low_mask = nbuckets - 1;
	hashp->hdr.high_mask = (nbuckets << 1) - 1;

	/*
	 * The number of header pages is the size of the header divided by
	 * the amount of freespace on header pages (the page size - the
	 * size of 1 integer where the length of the header info on that
	 * page is stored) plus another page if it didn't divide evenly.
	 */
	hashp->hdr.hdrpages =
	    (sizeof(HASHHDR) / (hashp->hdr.bsize - HEADER_OVERHEAD)) +
	    (((sizeof(HASHHDR) % (hashp->hdr.bsize - HEADER_OVERHEAD)) == 0)
	    ? 0 : 1);

	/* Create pages for these buckets */
	/*
	for (i = 0; i <= hashp->hdr.max_bucket; i++) {
		if (__new_page(hashp, (u_int32_t)i, A_BUCKET) != 0)
			return (-1);
	}
	*/

	/* First bitmap page is at: splitpoint l2 page offset 1 */
	if (__ibitmap(hashp, OADDR_OF(l2, 1), l2 + 1, 0))
		return (-1);

	return (0);
}

/*
 * Functions to get/put hash header.  We access the file directly.
 */
static u_int32_t
hget_header(hashp, page_size)
	HTAB *hashp;
	u_int32_t page_size;
{
	u_int32_t num_copied;
	u_int8_t *hdr_dest;

	num_copied = 0;

	hdr_dest = (u_int8_t *)&hashp->hdr;

	/*
	 * XXX
	 * This should not be printing to stderr on a "normal" error case.
	 */
	lseek(hashp->fp, 0, SEEK_SET);
	num_copied = read(hashp->fp, hdr_dest, sizeof(HASHHDR));
	if (num_copied != sizeof(HASHHDR)) {
		fprintf(stderr, "hash: could not retrieve header");
		return (0);
	}
#if DB_BYTE_ORDER == DB_LITTLE_ENDIAN
	swap_header(hashp);
#endif
	return (num_copied);
}

static void
hput_header(hashp)
	HTAB *hashp;
{
	HASHHDR *whdrp;
#if DB_BYTE_ORDER == DB_LITTLE_ENDIAN
	HASHHDR whdr;
#endif
	u_int32_t num_copied;

	num_copied = 0;

	whdrp = &hashp->hdr;
#if DB_BYTE_ORDER == DB_LITTLE_ENDIAN
	whdrp = &whdr;
	swap_header_copy(&hashp->hdr, whdrp);
#endif

	lseek(hashp->fp, 0, SEEK_SET);
	num_copied = write(hashp->fp, whdrp, sizeof(HASHHDR));
	if (num_copied != sizeof(HASHHDR))
		(void)fprintf(stderr, "hash: could not write hash header");
	return;
}

/********************** DESTROY/CLOSE ROUTINES ************************/

/*
 * Flushes any changes to the file if necessary and destroys the hashp
 * structure, freeing all allocated space.
 */
static int32_t
hdestroy(hashp)
	HTAB *hashp;
{
	int32_t save_errno;

	save_errno = 0;

#ifdef HASH_STATISTICS
	{ int i;
	(void)fprintf(stderr, "hdestroy: accesses %ld collisions %ld\n",
	    hash_accesses, hash_collisions);
	(void)fprintf(stderr,
	    "hdestroy: expansions %ld\n", hash_expansions);
	(void)fprintf(stderr,
	    "hdestroy: overflows %ld\n", hash_overflows);
	(void)fprintf(stderr,
	    "hdestroy: big key/data pages %ld\n", hash_bigpages);
	(void)fprintf(stderr,
	    "keys %ld maxp %d\n", hashp->hdr.nkeys, hashp->hdr.max_bucket);

	for (i = 0; i < NCACHED; i++)
		(void)fprintf(stderr,
		    "spares[%d] = %d\n", i, hashp->hdr.spares[i]);
	}
#endif

	if (flush_meta(hashp) && !save_errno)
		save_errno = errno;

	/* Free the split page */
	if (hashp->split_buf)
		free(hashp->split_buf);

	/* Free the big key and big data returns */
	if (hashp->bigkey_buf)
		free(hashp->bigkey_buf);
	if (hashp->bigdata_buf)
		free(hashp->bigdata_buf);

	/* XXX This should really iterate over the cursor queue, but
	   it's not clear how to do that, and the only cursor a hash
	   table ever creates is the one used by hash_seq().  Passing
	   NULL as the first arg is also a kludge, but I know that
	   it's never used, so I do it.  The intent is to plug the
	   memory leak.  Correctness can come later. */

	if (hashp->seq_cursor)
		hashp->seq_cursor->delete(NULL, hashp->seq_cursor, 0);

	/* shut down mpool */
	mpool_sync(hashp->mp);
	mpool_close(hashp->mp);

	if (hashp->fp != -1)
		(void)close(hashp->fp);

	/*
	 * *** This may cause problems if hashp->fname is set in any case
	 * other than the case that we are generating a temporary file name.
	 * Note that the new version of mpool should support temporary
	 * files within mpool itself.
	 */
	if (hashp->fname && !hashp->save_file) {
#ifdef DEBUG
		fprintf(stderr, "Unlinking file %s.\n", hashp->fname);
#endif
		/* we need to chmod the file to allow it to be deleted... */
		chmod(hashp->fname, 0700);
		unlink(hashp->fname);
	}
	free(hashp);

	if (save_errno) {
		errno = save_errno;
		return (ERROR);
	}
	return (SUCCESS);
}

/*
 * Write modified pages to disk
 *
 * Returns:
 *	 0 == OK
 *	-1 ERROR
 */
static int32_t
hash_sync(dbp, flags)
	const DB *dbp;
	u_int32_t flags;
{
	HTAB *hashp;

	hashp = (HTAB *)dbp->internal;

	/*
	 * XXX
	 * Check success/failure conditions.
	 */
	return (flush_meta(hashp) || mpool_sync(hashp->mp));
}

/*
 * Returns:
 *	 0 == OK
 *	-1 indicates that errno should be set
 */
static int32_t
flush_meta(hashp)
	HTAB *hashp;
{
	int32_t i;

	if (!hashp->save_file)
		return (0);
	hashp->hdr.magic = HASHMAGIC;
	hashp->hdr.version = HASHVERSION;
	hashp->hdr.h_charkey = hashp->hash(CHARKEY, sizeof(CHARKEY));

	/* write out metadata */
	hput_header(hashp);

	for (i = 0; i < NCACHED; i++)
		if (hashp->mapp[i]) {
			if (__put_page(hashp,
			    (PAGE16 *)hashp->mapp[i], A_BITMAP, 1))
				return (-1);
			hashp->mapp[i] = NULL;
		}
	return (0);
}

/*******************************SEARCH ROUTINES *****************************/
/*
 * All the access routines return
 *
 * Returns:
 *	 0 on SUCCESS
 *	 1 to indicate an external ERROR (i.e. key not found, etc)
 *	-1 to indicate an internal ERROR (i.e. out of memory, etc)
 */

/* *** make sure this is true! */

static int32_t
hash_get(dbp, key, data, flag)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int32_t flag;
{
	HTAB *hashp;

	hashp = (HTAB *)dbp->internal;
	if (flag) {
		hashp->local_errno = errno = EINVAL;
		return (ERROR);
	}
	return (hash_access(hashp, HASH_GET, key, data));
}

static int32_t
hash_put(dbp, key, data, flag)
	const DB *dbp;
	DBT *key;
	const DBT *data;
	u_int32_t flag;
{
	HTAB *hashp;

	hashp = (HTAB *)dbp->internal;
	if (flag && flag != R_NOOVERWRITE) {
		hashp->local_errno = errno = EINVAL;
		return (ERROR);
	}
	if ((hashp->flags & O_ACCMODE) == O_RDONLY) {
		hashp->local_errno = errno = EPERM;
		return (ERROR);
	}
	return (hash_access(hashp, flag == R_NOOVERWRITE ?
		HASH_PUTNEW : HASH_PUT, key, (DBT *)data));
}

static int32_t
hash_delete(dbp, key, flag)
	const DB *dbp;
	const DBT *key;
	u_int32_t flag;		/* Ignored */
{
	HTAB *hashp;

	hashp = (HTAB *)dbp->internal;
	if (flag) {
		hashp->local_errno = errno = EINVAL;
		return (ERROR);
	}
	if ((hashp->flags & O_ACCMODE) == O_RDONLY) {
		hashp->local_errno = errno = EPERM;
		return (ERROR);
	}

	return (hash_access(hashp, HASH_DELETE, key, NULL));
}

/*
 * Assume that hashp has been set in wrapper routine.
 */
static int32_t
hash_access(hashp, action, key, val)
	HTAB *hashp;
	ACTION action;
	const DBT *key;
	DBT *val;
{
	DBT page_key, page_val;
	CURSOR cursor;
	ITEM_INFO item_info;
	u_int32_t bucket;
	u_int32_t num_items;

#ifdef HASH_STATISTICS
	hash_accesses++;
#endif

	num_items = 0;

	/*
	 * Set up item_info so that we're looking for space to add an item
	 * as we cycle through the pages looking for the key.
	 */
	if (action == HASH_PUT || action == HASH_PUTNEW) {
		if (ISBIG(key->size + val->size, hashp))
			item_info.seek_size = PAIR_OVERHEAD;
		else
			item_info.seek_size = key->size + val->size;
	} else
		item_info.seek_size = 0;
	item_info.seek_found_page = 0;

	bucket = __call_hash(hashp, (int8_t *)key->data, key->size);

	cursor.pagep = NULL;
	__get_item_reset(hashp, &cursor);

	cursor.bucket = bucket;
	while (1) {
		__get_item_next(hashp, &cursor, &page_key, &page_val, &item_info);
		if (item_info.status == ITEM_ERROR)
			return (ABNORMAL);
		if (item_info.status == ITEM_NO_MORE)
			break;
		num_items++;
		if (item_info.key_off == BIGPAIR) {
			/*
			 * !!!
			 * 0 is a valid index.
			 */
			if (__find_bigpair(hashp, &cursor, (int8_t *)key->data,
			    key->size) > 0)
				goto found;
		} else if (key->size == page_key.size &&
		    !memcmp(key->data, page_key.data, key->size))
			goto found;
	}
#ifdef HASH_STATISTICS
	hash_collisions++;
#endif
	__get_item_done(hashp, &cursor);

	/*
	 * At this point, item_info will list either the last page in
	 * the chain, or the last page in the chain plus a pgno for where
	 * to find the first page in the chain with space for the
	 * item we wish to add.
	 */

	/* Not found */
	switch (action) {
	case HASH_PUT:
	case HASH_PUTNEW:
		if (__addel(hashp, &item_info, key, val, num_items, 0))
			return (ERROR);
		break;
	case HASH_GET:
	case HASH_DELETE:
	default:
		return (ABNORMAL);
	}

	if (item_info.caused_expand)
		__expand_table(hashp);
	return (SUCCESS);

found:	__get_item_done(hashp, &cursor);

	switch (action) {
	case HASH_PUTNEW:
		/* mpool_put(hashp->mp, pagep, 0); */
		return (ABNORMAL);
	case HASH_GET:
		if (item_info.key_off == BIGPAIR) {
			if (__big_return(hashp, &item_info, val, 0))
				return (ERROR);
		} else {
			val->data = page_val.data;
			val->size = page_val.size;
		}
		/* *** data may not be available! */
		break;
	case HASH_PUT:
		if (__delpair(hashp, &cursor, &item_info) ||
		    __addel(hashp, &item_info, key, val, UNKNOWN, 0))
			return (ERROR);
		__get_item_done(hashp, &cursor);
		if (item_info.caused_expand)
			__expand_table(hashp);
		break;
	case HASH_DELETE:
		if (__delpair(hashp, &cursor, &item_info))
			return (ERROR);
		break;
	default:
		abort();
	}
	return (SUCCESS);
}

/* ****************** CURSORS ********************************** */
CURSOR *
__cursor_creat(dbp)
	const DB *dbp;
{
	CURSOR *new_curs;
	HTAB *hashp;

	new_curs = (CURSOR *)malloc(sizeof(struct cursor_t));
	if (!new_curs)
		return NULL;
	new_curs->internal =
	    (struct item_info *)malloc(sizeof(struct item_info));
	if (!new_curs->internal) {
		free(new_curs);
		return NULL;
	}
	new_curs->get = cursor_get;
	new_curs->delete = cursor_delete;

	new_curs->bucket = 0;
	new_curs->pgno = INVALID_PGNO;
	new_curs->ndx = 0;
	new_curs->pgndx = 0;
	new_curs->pagep = NULL;

	/* place onto queue of cursors */
	hashp = (HTAB *)dbp->internal;
	TAILQ_INSERT_TAIL(&hashp->curs_queue, new_curs, queue);

	return new_curs;
}

static int32_t
cursor_get(dbp, cursorp, key, val, flags)
	const DB *dbp;
	CURSOR *cursorp;
	DBT *key, *val;
	u_int32_t flags;
{
	HTAB *hashp;
	ITEM_INFO item_info;

	hashp = (HTAB *)dbp->internal;

	if (flags && flags != R_FIRST && flags != R_NEXT) {
		hashp->local_errno = errno = EINVAL;
		return (ERROR);
	}
#ifdef HASH_STATISTICS
	hash_accesses++;
#endif

	item_info.seek_size = 0;

	if (flags == R_FIRST)
		__get_item_first(hashp, cursorp, key, val, &item_info);
	else
		__get_item_next(hashp, cursorp, key, val, &item_info);

	/*
	 * This needs to be changed around.  As is, get_item_next advances
	 * the pointers on the page but this function actually advances
	 * bucket pointers.  This works, since the only other place we
	 * use get_item_next is in hash_access which only deals with one
	 * bucket at a time.  However, there is the problem that certain other
	 * functions (such as find_bigpair and delpair) depend on the
	 * pgndx member of the cursor.  Right now, they are using pngdx - 1
	 * since indices refer to the __next__ item that is to be fetched
	 * from the page.  This is ugly, as you may have noticed, whoever
	 * you are.  The best solution would be to depend on item_infos to
	 * deal with _current_ information, and have the cursors only
	 * deal with _next_ information.  In that scheme, get_item_next
	 * would also advance buckets.  Version 3...
	 */


	/*
	 * Must always enter this loop to do error handling and
	 * check for big key/data pair.
	 */
	while (1) {
		if (item_info.status == ITEM_OK) {
			if (item_info.key_off == BIGPAIR &&
			    __big_keydata(hashp, cursorp->pagep, key, val,
			    item_info.pgndx))
				return (ABNORMAL);

			break;
		} else if (item_info.status != ITEM_NO_MORE)
			return (ABNORMAL);

		__put_page(hashp, cursorp->pagep, A_RAW, 0);
		cursorp->ndx = cursorp->pgndx = 0;
		cursorp->bucket++;
		cursorp->pgno = INVALID_PGNO;
		cursorp->pagep = NULL;
		if (cursorp->bucket > hashp->hdr.max_bucket)
			return (ABNORMAL);
		__get_item_next(hashp, cursorp, key, val, &item_info);
	}

	__get_item_done(hashp, cursorp);
	return (0);
}

static int32_t
cursor_delete(dbp, cursor, flags)
	const DB *dbp;
	CURSOR *cursor;
	u_int32_t flags;
{
	/* XXX this is empirically determined, so it might not be completely
	   correct, but it seems to work.  At the very least it fixes
	   a memory leak */

	free(cursor->internal);
	free(cursor);

	return (0);
}

static int32_t
hash_seq(dbp, key, val, flag)
	const DB *dbp;
	DBT *key, *val;
	u_int32_t flag;
{
	HTAB *hashp;

	/*
	 * Seq just uses the default cursor to go sequecing through the
	 * database.  Note that the default cursor is the first in the list.
	 */

	hashp = (HTAB *)dbp->internal;
	if (!hashp->seq_cursor)
		hashp->seq_cursor = __cursor_creat(dbp);

	return (hashp->seq_cursor->get(dbp, hashp->seq_cursor, key, val, flag));
}

/********************************* UTILITIES ************************/

/*
 * Returns:
 *	 0 ==> OK
 *	-1 ==> Error
 */
int32_t
__expand_table(hashp)
	HTAB *hashp;
{
	u_int32_t old_bucket, new_bucket;
	int32_t spare_ndx;

#ifdef HASH_STATISTICS
	hash_expansions++;
#endif
	new_bucket = ++hashp->hdr.max_bucket;
	old_bucket = (hashp->hdr.max_bucket & hashp->hdr.low_mask);

	/* Get a page for this new bucket */
	if (__new_page(hashp, new_bucket, A_BUCKET) != 0)
		return (-1);

	/*
	 * If the split point is increasing (hdr.max_bucket's log base 2
	 * increases), we need to copy the current contents of the spare
	 * split bucket to the next bucket.
	 */
	spare_ndx = __log2(hashp->hdr.max_bucket + 1);
	if (spare_ndx > hashp->hdr.ovfl_point) {
		hashp->hdr.spares[spare_ndx] = hashp->hdr.spares[hashp->hdr.ovfl_point];
		hashp->hdr.ovfl_point = spare_ndx;
	}
	if (new_bucket > hashp->hdr.high_mask) {
		/* Starting a new doubling */
		hashp->hdr.low_mask = hashp->hdr.high_mask;
		hashp->hdr.high_mask = new_bucket | hashp->hdr.low_mask;
	}
	if (BUCKET_TO_PAGE(new_bucket) > MAX_PAGES(hashp)) {
		fprintf(stderr, "hash: Cannot allocate new bucket.  Pages exhausted.\n");
		return (-1);
	}
	/* Relocate records to the new bucket */
	return (__split_page(hashp, old_bucket, new_bucket));
}

u_int32_t
__call_hash(hashp, k, len)
	HTAB *hashp;
	int8_t *k;
	int32_t len;
{
	u_int32_t n, bucket;

	n = hashp->hash(k, len);
	bucket = n & hashp->hdr.high_mask;
	if (bucket > hashp->hdr.max_bucket)
		bucket = bucket & hashp->hdr.low_mask;
	return (bucket);
}

#if DB_BYTE_ORDER == DB_LITTLE_ENDIAN
/*
 * Hashp->hdr needs to be byteswapped.
 */
static void
swap_header_copy(srcp, destp)
	HASHHDR *srcp, *destp;
{
	int32_t i;

	P_32_COPY(srcp->magic, destp->magic);
	P_32_COPY(srcp->version, destp->version);
	P_32_COPY(srcp->lorder, destp->lorder);
	P_32_COPY(srcp->bsize, destp->bsize);
	P_32_COPY(srcp->bshift, destp->bshift);
	P_32_COPY(srcp->ovfl_point, destp->ovfl_point);
	P_32_COPY(srcp->last_freed, destp->last_freed);
	P_32_COPY(srcp->max_bucket, destp->max_bucket);
	P_32_COPY(srcp->high_mask, destp->high_mask);
	P_32_COPY(srcp->low_mask, destp->low_mask);
	P_32_COPY(srcp->ffactor, destp->ffactor);
	P_32_COPY(srcp->nkeys, destp->nkeys);
	P_32_COPY(srcp->hdrpages, destp->hdrpages);
	P_32_COPY(srcp->h_charkey, destp->h_charkey);
	for (i = 0; i < NCACHED; i++) {
		P_32_COPY(srcp->spares[i], destp->spares[i]);
		P_16_COPY(srcp->bitmaps[i], destp->bitmaps[i]);
	}
}

static void
swap_header(hashp)
	HTAB *hashp;
{
	HASHHDR *hdrp;
	int32_t i;

	hdrp = &hashp->hdr;

	M_32_SWAP(hdrp->magic);
	M_32_SWAP(hdrp->version);
	M_32_SWAP(hdrp->lorder);
	M_32_SWAP(hdrp->bsize);
	M_32_SWAP(hdrp->bshift);
	M_32_SWAP(hdrp->ovfl_point);
	M_32_SWAP(hdrp->last_freed);
	M_32_SWAP(hdrp->max_bucket);
	M_32_SWAP(hdrp->high_mask);
	M_32_SWAP(hdrp->low_mask);
	M_32_SWAP(hdrp->ffactor);
	M_32_SWAP(hdrp->nkeys);
	M_32_SWAP(hdrp->hdrpages);
	M_32_SWAP(hdrp->h_charkey);
	for (i = 0; i < NCACHED; i++) {
		M_32_SWAP(hdrp->spares[i]);
		M_16_SWAP(hdrp->bitmaps[i]);
	}
}
#endif /* DB_BYTE_ORDER == DB_LITTLE_ENDIAN */
