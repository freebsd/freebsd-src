/*	$NetBSD: rf_memchunk.c,v 1.4 1999/08/13 03:41:56 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*********************************************************************************
 * rf_memchunk.c
 *
 * experimental code.  I've found that the malloc and free calls in the DAG
 * creation code are very expensive.  Since for any given workload the DAGs
 * created for different accesses are likely to be similar to each other, the
 * amount of memory used for any given DAG data structure is likely to be one
 * of a small number of values.  For example, in UNIX, all reads and writes will
 * be less than 8k and will not span stripe unit boundaries.  Thus in the absence
 * of failure, the only DAGs that will ever get created are single-node reads
 * and single-stripe-unit atomic read-modify-writes.  So, I'm very likely to
 * be continually asking for chunks of memory equal to the sizes of these two
 * DAGs.
 *
 * This leads to the idea of holding on to these chunks of memory when the DAG is
 * freed and then, when a new DAG is created, trying to find such a chunk before
 * calling malloc.
 *
 * the "chunk list" is a list of lists.  Each header node contains a size value
 * and a pointer to a list of chunk descriptors, each of which holds a pointer
 * to a chunk of memory of the indicated size.
 *
 * There is currently no way to purge memory out of the chunk list.  My
 * initial thought on this is to have a low-priority thread that wakes up every
 * 1 or 2 seconds, purges all the chunks with low reuse counts, and sets all
 * the reuse counts to zero.
 *
 * This whole idea may be bad, since malloc may be able to do this more efficiently.
 * It's worth a try, though, and it can be turned off by setting useMemChunks to 0.
 *
 ********************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_memchunk.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_options.h>
#include <dev/raidframe/rf_shutdown.h>

typedef struct RF_ChunkHdr_s RF_ChunkHdr_t;
struct RF_ChunkHdr_s {
	int     size;
	RF_ChunkDesc_t *list;
	RF_ChunkHdr_t *next;
};

static RF_ChunkHdr_t *chunklist, *chunk_hdr_free_list;
static RF_ChunkDesc_t *chunk_desc_free_list;
RF_DECLARE_STATIC_MUTEX(chunkmutex)
	static void rf_ShutdownMemChunk(void *);
	static RF_ChunkDesc_t *NewMemChunk(int, char *);


	static void rf_ShutdownMemChunk(ignored)
	void   *ignored;
{
	RF_ChunkDesc_t *pt, *p;
	RF_ChunkHdr_t *hdr, *ht;

	if (rf_memChunkDebug)
		printf("Chunklist:\n");
	for (hdr = chunklist; hdr;) {
		for (p = hdr->list; p;) {
			if (rf_memChunkDebug)
				printf("Size %d reuse count %d\n", p->size, p->reuse_count);
			pt = p;
			p = p->next;
			RF_Free(pt->buf, pt->size);
			RF_Free(pt, sizeof(*pt));
		}
		ht = hdr;
		hdr = hdr->next;
		RF_Free(ht, sizeof(*ht));
	}

	rf_mutex_destroy(&chunkmutex);
}

int 
rf_ConfigureMemChunk(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	chunklist = NULL;
	chunk_hdr_free_list = NULL;
	chunk_desc_free_list = NULL;
	rc = rf_mutex_init(&chunkmutex, __FUNCTION__);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownMemChunk, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_mutex_destroy(&chunkmutex);
	}
	return (rc);
}
/* called to get a chunk descriptor for a newly-allocated chunk of memory
 * MUTEX MUST BE LOCKED
 *
 * free list is not currently used
 */
static RF_ChunkDesc_t *
NewMemChunk(size, buf)
	int     size;
	char   *buf;
{
	RF_ChunkDesc_t *p;

	if (chunk_desc_free_list) {
		p = chunk_desc_free_list;
		chunk_desc_free_list = p->next;
	} else
		RF_Malloc(p, sizeof(RF_ChunkDesc_t), (RF_ChunkDesc_t *));
	p->size = size;
	p->buf = buf;
	p->next = NULL;
	p->reuse_count = 0;
	return (p);
}
/* looks for a chunk of memory of acceptable size.  If none, allocates one and returns
 * a chunk descriptor for it, but does not install anything in the list.  This is done
 * when the chunk is released.
 */
RF_ChunkDesc_t *
rf_GetMemChunk(size)
	int     size;
{
	RF_ChunkHdr_t *hdr = chunklist;
	RF_ChunkDesc_t *p = NULL;
	char   *buf;

	RF_LOCK_MUTEX(chunkmutex);
	for (hdr = chunklist; hdr; hdr = hdr->next)
		if (hdr->size >= size) {
			p = hdr->list;
			if (p) {
				hdr->list = p->next;
				p->next = NULL;
				p->reuse_count++;
			}
			break;
		}
	if (!p) {
		RF_Malloc(buf, size, (char *));
		p = NewMemChunk(size, buf);
	}
	RF_UNLOCK_MUTEX(chunkmutex);
	(void) bzero(p->buf, size);
	return (p);
}

void 
rf_ReleaseMemChunk(chunk)
	RF_ChunkDesc_t *chunk;
{
	RF_ChunkHdr_t *hdr, *ht = NULL, *new;

	RF_LOCK_MUTEX(chunkmutex);
	for (hdr = chunklist; hdr && hdr->size < chunk->size; ht = hdr, hdr = hdr->next);
	if (hdr && hdr->size == chunk->size) {
		chunk->next = hdr->list;
		hdr->list = chunk;
	} else {
		RF_Malloc(new, sizeof(RF_ChunkHdr_t), (RF_ChunkHdr_t *));
		new->size = chunk->size;
		new->list = chunk;
		chunk->next = NULL;
		if (ht) {
			new->next = ht->next;
			ht->next = new;
		} else {
			new->next = hdr;
			chunklist = new;
		}
	}
	RF_UNLOCK_MUTEX(chunkmutex);
}
