/*	$FreeBSD$ */
/*	$NetBSD: rf_stripelocks.c,v 1.6 2000/12/04 11:35:46 fvdl Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, Jim Zelenka
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

/*
 * stripelocks.c -- code to lock stripes for read and write access
 *
 * The code distinguishes between read locks and write locks. There can be
 * as many readers to given stripe as desired. When a write request comes
 * in, no further readers are allowed to enter, and all subsequent requests
 * are queued in FIFO order. When a the number of readers goes to zero, the
 * writer is given the lock. When a writer releases the lock, the list of
 * queued requests is scanned, and all readersq up to the next writer are
 * given the lock.
 *
 * The lock table size must be one less than a power of two, but HASH_STRIPEID
 * is the only function that requires this.
 *
 * The code now supports "range locks". When you ask to lock a stripe, you
 * specify a range of addresses in that stripe that you want to lock. When
 * you acquire the lock, you've locked only this range of addresses, and
 * other threads can concurrently read/write any non-overlapping portions
 * of the stripe. The "addresses" that you lock are abstract in that you
 * can pass in anything you like.  The expectation is that you'll pass in
 * the range of physical disk offsets of the parity bits you're planning
 * to update. The idea behind this, of course, is to allow sub-stripe
 * locking. The implementation is perhaps not the best imaginable; in the
 * worst case a lock release is O(n^2) in the total number of outstanding
 * requests to a given stripe.  Note that if you're striping with a
 * stripe unit size equal to an entire disk (i.e. not striping), there will
 * be only one stripe and you may spend some significant number of cycles
 * searching through stripe lock descriptors.
 */

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_stripelocks.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_debugprint.h>
#include <dev/raidframe/rf_driver.h>
#include <dev/raidframe/rf_shutdown.h>

#define Dprintf1(s,a)         rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)       rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c)     rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),NULL,NULL,NULL,NULL,NULL)
#define Dprintf4(s,a,b,c,d)   rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),NULL,NULL,NULL,NULL)
#define Dprintf5(s,a,b,c,d,e) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),NULL,NULL,NULL)
#define Dprintf6(s,a,b,c,d,e,f) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),(void *)((unsigned long)f),NULL,NULL)
#define Dprintf7(s,a,b,c,d,e,f,g) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),(void *)((unsigned long)f),(void *)((unsigned long)g),NULL)
#define Dprintf8(s,a,b,c,d,e,f,g,h) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),(void *)((unsigned long)d),(void *)((unsigned long)e),(void *)((unsigned long)f),(void *)((unsigned long)g),(void *)((unsigned long)h))

#define FLUSH

#define HASH_STRIPEID(_sid_)  ( (_sid_) & (rf_lockTableSize-1) )

static void AddToWaitersQueue(RF_LockTableEntry_t * lockTable, RF_StripeLockDesc_t * lockDesc, RF_LockReqDesc_t * lockReqDesc);
static RF_StripeLockDesc_t *AllocStripeLockDesc(RF_StripeNum_t stripeID);
static void FreeStripeLockDesc(RF_StripeLockDesc_t * p);
static void PrintLockedStripes(RF_LockTableEntry_t * lockTable);

/* determines if two ranges overlap.  always yields false if either start value is negative  */
#define SINGLE_RANGE_OVERLAP(_strt1, _stop1, _strt2, _stop2)                                     \
  ( (_strt1 >= 0) && (_strt2 >= 0) && (RF_MAX(_strt1, _strt2) <= RF_MIN(_stop1, _stop2)) )

/* determines if any of the ranges specified in the two lock descriptors overlap each other */
#define RANGE_OVERLAP(_cand, _pred)                                                  \
  ( SINGLE_RANGE_OVERLAP((_cand)->start,  (_cand)->stop,  (_pred)->start,  (_pred)->stop ) ||    \
    SINGLE_RANGE_OVERLAP((_cand)->start2, (_cand)->stop2, (_pred)->start,  (_pred)->stop ) ||    \
    SINGLE_RANGE_OVERLAP((_cand)->start,  (_cand)->stop,  (_pred)->start2, (_pred)->stop2) ||    \
    SINGLE_RANGE_OVERLAP((_cand)->start2, (_cand)->stop2, (_pred)->start2, (_pred)->stop2) )

/* Determines if a candidate lock request conflicts with a predecessor lock req.
 * Note that the arguments are not interchangeable.
 * The rules are:
 *      a candidate read conflicts with a predecessor write if any ranges overlap
 *      a candidate write conflicts with a predecessor read if any ranges overlap
 *      a candidate write conflicts with a predecessor write if any ranges overlap
 */
#define STRIPELOCK_CONFLICT(_cand, _pred)                                        \
  RANGE_OVERLAP((_cand), (_pred)) &&                                        \
  ( ( (((_cand)->type == RF_IO_TYPE_READ) && ((_pred)->type == RF_IO_TYPE_WRITE)) ||                      \
      (((_cand)->type == RF_IO_TYPE_WRITE) && ((_pred)->type == RF_IO_TYPE_READ)) ||                      \
      (((_cand)->type == RF_IO_TYPE_WRITE) && ((_pred)->type == RF_IO_TYPE_WRITE))                         \
    )                                                                            \
  )

static RF_FreeList_t *rf_stripelock_freelist;
#define RF_MAX_FREE_STRIPELOCK 128
#define RF_STRIPELOCK_INC        8
#define RF_STRIPELOCK_INITIAL   32

static void rf_ShutdownStripeLockFreeList(void *);
static void rf_RaidShutdownStripeLocks(void *);

static void 
rf_ShutdownStripeLockFreeList(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY(rf_stripelock_freelist, next, (RF_StripeLockDesc_t *));
}

int 
rf_ConfigureStripeLockFreeList(listp)
	RF_ShutdownList_t **listp;
{
	unsigned mask;
	int     rc;

	RF_FREELIST_CREATE(rf_stripelock_freelist, RF_MAX_FREE_STRIPELOCK,
	    RF_STRIPELOCK_INITIAL, sizeof(RF_StripeLockDesc_t));
	rc = rf_ShutdownCreate(listp, rf_ShutdownStripeLockFreeList, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_ShutdownStripeLockFreeList(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME(rf_stripelock_freelist, RF_STRIPELOCK_INITIAL, next,
	    (RF_StripeLockDesc_t *));
	for (mask = 0x1; mask; mask <<= 1)
		if (rf_lockTableSize == mask)
			break;
	if (!mask) {
		printf("[WARNING:  lock table size must be a power of two.  Setting to %d.]\n", RF_DEFAULT_LOCK_TABLE_SIZE);
		rf_lockTableSize = RF_DEFAULT_LOCK_TABLE_SIZE;
	}
	return (0);
}

RF_LockTableEntry_t *
rf_MakeLockTable()
{
	RF_LockTableEntry_t *lockTable;
	int     i, rc;

	RF_Calloc(lockTable, ((int) rf_lockTableSize), sizeof(RF_LockTableEntry_t), (RF_LockTableEntry_t *));
	if (lockTable == NULL)
		return (NULL);
	for (i = 0; i < rf_lockTableSize; i++) {
		rc = rf_mutex_init(&lockTable[i].mutex, __FUNCTION__);
		if (rc) {
			RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
			    __LINE__, rc);
			/* XXX clean up other mutexes */
			return (NULL);
		}
	}
	return (lockTable);
}

void 
rf_ShutdownStripeLocks(RF_LockTableEntry_t * lockTable)
{
	int     i;

	if (rf_stripeLockDebug) {
		PrintLockedStripes(lockTable);
	}
	for (i = 0; i < rf_lockTableSize; i++) {
		rf_mutex_destroy(&lockTable[i].mutex);
	}
	RF_Free(lockTable, rf_lockTableSize * sizeof(RF_LockTableEntry_t));
}

static void 
rf_RaidShutdownStripeLocks(arg)
	void   *arg;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) arg;
	rf_ShutdownStripeLocks(raidPtr->lockTable);
}

int 
rf_ConfigureStripeLocks(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	int     rc;

	raidPtr->lockTable = rf_MakeLockTable();
	if (raidPtr->lockTable == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_RaidShutdownStripeLocks, raidPtr);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_ShutdownStripeLocks(raidPtr->lockTable);
		return (rc);
	}
	return (0);
}
/* returns 0 if you've got the lock, and non-zero if you have to wait.
 * if and only if you have to wait, we'll cause cbFunc to get invoked
 * with cbArg when you are granted the lock.  We store a tag in *releaseTag
 * that you need to give back to us when you release the lock.
 */
int 
rf_AcquireStripeLock(
    RF_LockTableEntry_t * lockTable,
    RF_StripeNum_t stripeID,
    RF_LockReqDesc_t * lockReqDesc)
{
	RF_StripeLockDesc_t *lockDesc;
	RF_LockReqDesc_t *p;
	int     tid = 0, hashval = HASH_STRIPEID(stripeID);
	int     retcode = 0;

	RF_ASSERT(RF_IO_IS_R_OR_W(lockReqDesc->type));

	if (rf_stripeLockDebug) {
		if (stripeID == -1)
			Dprintf1("[%d] Lock acquisition supressed (stripeID == -1)\n", tid);
		else {
			Dprintf8("[%d] Trying to acquire stripe lock table 0x%lx SID %ld type %c range %ld-%ld, range2 %ld-%ld hashval %d\n",
			    tid, (unsigned long) lockTable, stripeID, lockReqDesc->type, lockReqDesc->start,
			    lockReqDesc->stop, lockReqDesc->start2, lockReqDesc->stop2);
			Dprintf3("[%d] lock %ld hashval %d\n", tid, stripeID, hashval);
			FLUSH;
		}
	}
	if (stripeID == -1)
		return (0);
	lockReqDesc->next = NULL;	/* just to be sure */

	RF_LOCK_MUTEX(lockTable[hashval].mutex);
	for (lockDesc = lockTable[hashval].descList; lockDesc; lockDesc = lockDesc->next) {
		if (lockDesc->stripeID == stripeID)
			break;
	}

	if (!lockDesc) {	/* no entry in table => no one reading or
				 * writing */
		lockDesc = AllocStripeLockDesc(stripeID);
		lockDesc->next = lockTable[hashval].descList;
		lockTable[hashval].descList = lockDesc;
		if (lockReqDesc->type == RF_IO_TYPE_WRITE)
			lockDesc->nWriters++;
		lockDesc->granted = lockReqDesc;
		if (rf_stripeLockDebug) {
			Dprintf7("[%d] no one waiting: lock %ld %c %ld-%ld %ld-%ld granted\n",
			    tid, stripeID, lockReqDesc->type, lockReqDesc->start, lockReqDesc->stop, lockReqDesc->start2, lockReqDesc->stop2);
			FLUSH;
		}
	} else {

		if (lockReqDesc->type == RF_IO_TYPE_WRITE)
			lockDesc->nWriters++;

		if (lockDesc->nWriters == 0) {	/* no need to search any lists
						 * if there are no writers
						 * anywhere */
			lockReqDesc->next = lockDesc->granted;
			lockDesc->granted = lockReqDesc;
			if (rf_stripeLockDebug) {
				Dprintf7("[%d] no writers: lock %ld %c %ld-%ld %ld-%ld granted\n",
				    tid, stripeID, lockReqDesc->type, lockReqDesc->start, lockReqDesc->stop, lockReqDesc->start2, lockReqDesc->stop2);
				FLUSH;
			}
		} else {

			/* search the granted & waiting lists for a conflict.
			 * stop searching as soon as we find one */
			retcode = 0;
			for (p = lockDesc->granted; p; p = p->next)
				if (STRIPELOCK_CONFLICT(lockReqDesc, p)) {
					retcode = 1;
					break;
				}
			if (!retcode)
				for (p = lockDesc->waitersH; p; p = p->next)
					if (STRIPELOCK_CONFLICT(lockReqDesc, p)) {
						retcode = 2;
						break;
					}
			if (!retcode) {
				lockReqDesc->next = lockDesc->granted;	/* no conflicts found =>
									 * grant lock */
				lockDesc->granted = lockReqDesc;
				if (rf_stripeLockDebug) {
					Dprintf7("[%d] no conflicts: lock %ld %c %ld-%ld %ld-%ld granted\n",
					    tid, stripeID, lockReqDesc->type, lockReqDesc->start, lockReqDesc->stop,
					    lockReqDesc->start2, lockReqDesc->stop2);
					FLUSH;
				}
			} else {
				if (rf_stripeLockDebug) {
					Dprintf6("[%d] conflict: lock %ld %c %ld-%ld hashval=%d not granted\n",
					    tid, stripeID, lockReqDesc->type, lockReqDesc->start, lockReqDesc->stop,
					    hashval);
					Dprintf3("[%d] lock %ld retcode=%d\n", tid, stripeID, retcode);
					FLUSH;
				}
				AddToWaitersQueue(lockTable, lockDesc, lockReqDesc);	/* conflict => the
											 * current access must
											 * wait */
			}
		}
	}

	RF_UNLOCK_MUTEX(lockTable[hashval].mutex);
	return (retcode);
}

void 
rf_ReleaseStripeLock(
    RF_LockTableEntry_t * lockTable,
    RF_StripeNum_t stripeID,
    RF_LockReqDesc_t * lockReqDesc)
{
	RF_StripeLockDesc_t *lockDesc, *ld_t;
	RF_LockReqDesc_t *lr, *lr_t, *callbacklist, *t;
	RF_IoType_t type = lockReqDesc->type;
	int     tid = 0, hashval = HASH_STRIPEID(stripeID);
	int     release_it, consider_it;
	RF_LockReqDesc_t *candidate, *candidate_t, *predecessor;

	RF_ASSERT(RF_IO_IS_R_OR_W(type));

	if (rf_stripeLockDebug) {
		if (stripeID == -1)
			Dprintf1("[%d] Lock release supressed (stripeID == -1)\n", tid);
		else {
			Dprintf8("[%d] Releasing stripe lock on stripe ID %ld, type %c range %ld-%ld %ld-%ld table 0x%lx\n",
			    tid, stripeID, lockReqDesc->type, lockReqDesc->start, lockReqDesc->stop, lockReqDesc->start2, lockReqDesc->stop2, lockTable);
			FLUSH;
		}
	}
	if (stripeID == -1)
		return;

	RF_LOCK_MUTEX(lockTable[hashval].mutex);

	/* find the stripe lock descriptor */
	for (ld_t = NULL, lockDesc = lockTable[hashval].descList; lockDesc; ld_t = lockDesc, lockDesc = lockDesc->next) {
		if (lockDesc->stripeID == stripeID)
			break;
	}
	RF_ASSERT(lockDesc);	/* major error to release a lock that doesn't
				 * exist */

	/* find the stripe lock request descriptor & delete it from the list */
	for (lr_t = NULL, lr = lockDesc->granted; lr; lr_t = lr, lr = lr->next)
		if (lr == lockReqDesc)
			break;

	RF_ASSERT(lr && (lr == lockReqDesc));	/* major error to release a
						 * lock that hasn't been
						 * granted */
	if (lr_t)
		lr_t->next = lr->next;
	else {
		RF_ASSERT(lr == lockDesc->granted);
		lockDesc->granted = lr->next;
	}
	lr->next = NULL;

	if (lockReqDesc->type == RF_IO_TYPE_WRITE)
		lockDesc->nWriters--;

	/* search through the waiters list to see if anyone needs to be woken
	 * up. for each such descriptor in the wait list, we check it against
	 * everything granted and against everything _in front_ of it in the
	 * waiters queue.  If it conflicts with none of these, we release it.
	 * 
	 * DON'T TOUCH THE TEMPLINK POINTER OF ANYTHING IN THE GRANTED LIST HERE.
	 * This will roach the case where the callback tries to acquire a new
	 * lock in the same stripe.  There are some asserts to try and detect
	 * this.
	 * 
	 * We apply 2 performance optimizations: (1) if releasing this lock
	 * results in no more writers to this stripe, we just release
	 * everybody waiting, since we place no restrictions on the number of
	 * concurrent reads. (2) we consider as candidates for wakeup only
	 * those waiters that have a range overlap with either the descriptor
	 * being woken up or with something in the callbacklist (i.e.
	 * something we've just now woken up). This allows us to avoid the
	 * long evaluation for some descriptors. */

	callbacklist = NULL;
	if (lockDesc->nWriters == 0) {	/* performance tweak (1) */
		while (lockDesc->waitersH) {

			lr = lockDesc->waitersH;	/* delete from waiters
							 * list */
			lockDesc->waitersH = lr->next;

			RF_ASSERT(lr->type == RF_IO_TYPE_READ);

			lr->next = lockDesc->granted;	/* add to granted list */
			lockDesc->granted = lr;

			RF_ASSERT(!lr->templink);
			lr->templink = callbacklist;	/* put on callback list
							 * so that we'll invoke
							 * callback below */
			callbacklist = lr;
			if (rf_stripeLockDebug) {
				Dprintf8("[%d] No writers: granting lock stripe ID %ld, type %c range %ld-%ld %ld-%ld table 0x%lx\n",
				    tid, stripeID, lr->type, lr->start, lr->stop, lr->start2, lr->stop2, (unsigned long) lockTable);
				FLUSH;
			}
		}
		lockDesc->waitersT = NULL;	/* we've purged the whole
						 * waiters list */

	} else
		for (candidate_t = NULL, candidate = lockDesc->waitersH; candidate;) {

			/* performance tweak (2) */
			consider_it = 0;
			if (RANGE_OVERLAP(lockReqDesc, candidate))
				consider_it = 1;
			else
				for (t = callbacklist; t; t = t->templink)
					if (RANGE_OVERLAP(t, candidate)) {
						consider_it = 1;
						break;
					}
			if (!consider_it) {
				if (rf_stripeLockDebug) {
					Dprintf8("[%d] No overlap: rejecting candidate stripeID %ld, type %c range %ld-%ld %ld-%ld table 0x%lx\n",
					    tid, stripeID, candidate->type, candidate->start, candidate->stop, candidate->start2, candidate->stop2,
					    (unsigned long) lockTable);
					FLUSH;
				}
				candidate_t = candidate;
				candidate = candidate->next;
				continue;
			}
			/* we have a candidate for release.  check to make
			 * sure it is not blocked by any granted locks */
			release_it = 1;
			for (predecessor = lockDesc->granted; predecessor; predecessor = predecessor->next) {
				if (STRIPELOCK_CONFLICT(candidate, predecessor)) {
					if (rf_stripeLockDebug) {
						Dprintf8("[%d] Conflicts with granted lock: rejecting candidate stripeID %ld, type %c range %ld-%ld %ld-%ld table 0x%lx\n",
						    tid, stripeID, candidate->type, candidate->start, candidate->stop, candidate->start2, candidate->stop2,
						    (unsigned long) lockTable);
						FLUSH;
					}
					release_it = 0;
					break;
				}
			}

			/* now check to see if the candidate is blocked by any
			 * waiters that occur before it it the wait queue */
			if (release_it)
				for (predecessor = lockDesc->waitersH; predecessor != candidate; predecessor = predecessor->next) {
					if (STRIPELOCK_CONFLICT(candidate, predecessor)) {
						if (rf_stripeLockDebug) {
							Dprintf8("[%d] Conflicts with waiting lock: rejecting candidate stripeID %ld, type %c range %ld-%ld %ld-%ld table 0x%lx\n",
							    tid, stripeID, candidate->type, candidate->start, candidate->stop, candidate->start2, candidate->stop2,
							    (unsigned long) lockTable);
							FLUSH;
						}
						release_it = 0;
						break;
					}
				}

			/* release it if indicated */
			if (release_it) {
				if (rf_stripeLockDebug) {
					Dprintf8("[%d] Granting lock to candidate stripeID %ld, type %c range %ld-%ld %ld-%ld table 0x%lx\n",
					    tid, stripeID, candidate->type, candidate->start, candidate->stop, candidate->start2, candidate->stop2,
					    (unsigned long) lockTable);
					FLUSH;
				}
				if (candidate_t) {
					candidate_t->next = candidate->next;
					if (lockDesc->waitersT == candidate)
						lockDesc->waitersT = candidate_t;	/* cannot be waitersH
											 * since candidate_t is
											 * not NULL */
				} else {
					RF_ASSERT(candidate == lockDesc->waitersH);
					lockDesc->waitersH = lockDesc->waitersH->next;
					if (!lockDesc->waitersH)
						lockDesc->waitersT = NULL;
				}
				candidate->next = lockDesc->granted;	/* move it to the
									 * granted list */
				lockDesc->granted = candidate;

				RF_ASSERT(!candidate->templink);
				candidate->templink = callbacklist;	/* put it on the list of
									 * things to be called
									 * after we release the
									 * mutex */
				callbacklist = candidate;

				if (!candidate_t)
					candidate = lockDesc->waitersH;
				else
					candidate = candidate_t->next;	/* continue with the
									 * rest of the list */
			} else {
				candidate_t = candidate;
				candidate = candidate->next;	/* continue with the
								 * rest of the list */
			}
		}

	/* delete the descriptor if no one is waiting or active */
	if (!lockDesc->granted && !lockDesc->waitersH) {
		RF_ASSERT(lockDesc->nWriters == 0);
		if (rf_stripeLockDebug) {
			Dprintf3("[%d] Last lock released (table 0x%lx): deleting desc for stripeID %ld\n", tid, (unsigned long) lockTable, stripeID);
			FLUSH;
		}
		if (ld_t)
			ld_t->next = lockDesc->next;
		else {
			RF_ASSERT(lockDesc == lockTable[hashval].descList);
			lockTable[hashval].descList = lockDesc->next;
		}
		FreeStripeLockDesc(lockDesc);
		lockDesc = NULL;/* only for the ASSERT below */
	}
	RF_UNLOCK_MUTEX(lockTable[hashval].mutex);

	/* now that we've unlocked the mutex, invoke the callback on all the
	 * descriptors in the list */
	RF_ASSERT(!((callbacklist) && (!lockDesc)));	/* if we deleted the
							 * descriptor, we should
							 * have no callbacks to
							 * do */
	for (candidate = callbacklist; candidate;) {
		t = candidate;
		candidate = candidate->templink;
		t->templink = NULL;
		(t->cbFunc) (t->cbArg);
	}
}
/* must have the indicated lock table mutex upon entry */
static void 
AddToWaitersQueue(
    RF_LockTableEntry_t * lockTable,
    RF_StripeLockDesc_t * lockDesc,
    RF_LockReqDesc_t * lockReqDesc)
{
#if 0 /* XXX fvdl -- unitialized use of 'tid' */
	int     tid;

	if (rf_stripeLockDebug) {
		Dprintf3("[%d] Waiting on lock for stripe %ld table 0x%lx\n", tid, lockDesc->stripeID, (unsigned long) lockTable);
		FLUSH;
	}
#endif
	if (!lockDesc->waitersH) {
		lockDesc->waitersH = lockDesc->waitersT = lockReqDesc;
	} else {
		lockDesc->waitersT->next = lockReqDesc;
		lockDesc->waitersT = lockReqDesc;
	}
}

static RF_StripeLockDesc_t *
AllocStripeLockDesc(RF_StripeNum_t stripeID)
{
	RF_StripeLockDesc_t *p;

	RF_FREELIST_GET(rf_stripelock_freelist, p, next, (RF_StripeLockDesc_t *));
	if (p) {
		p->stripeID = stripeID;
	}
	return (p);
}

static void 
FreeStripeLockDesc(RF_StripeLockDesc_t * p)
{
	RF_FREELIST_FREE(rf_stripelock_freelist, p, next);
}

static void 
PrintLockedStripes(lockTable)
	RF_LockTableEntry_t *lockTable;
{
	int     i, j, foundone = 0, did;
	RF_StripeLockDesc_t *p;
	RF_LockReqDesc_t *q;

	RF_LOCK_MUTEX(rf_printf_mutex);
	printf("Locked stripes:\n");
	for (i = 0; i < rf_lockTableSize; i++)
		if (lockTable[i].descList) {
			foundone = 1;
			for (p = lockTable[i].descList; p; p = p->next) {
				printf("Stripe ID 0x%lx (%d) nWriters %d\n",
				    (long) p->stripeID, (int) p->stripeID, p->nWriters);

				if (!(p->granted))
					printf("Granted: (none)\n");
				else
					printf("Granted:\n");
				for (did = 1, j = 0, q = p->granted; q; j++, q = q->next) {
					printf("  %c(%ld-%ld", q->type, (long) q->start, (long) q->stop);
					if (q->start2 != -1)
						printf(",%ld-%ld) ", (long) q->start2,
						    (long) q->stop2);
					else
						printf(") ");
					if (j && !(j % 4)) {
						printf("\n");
						did = 1;
					} else
						did = 0;
				}
				if (!did)
					printf("\n");

				if (!(p->waitersH))
					printf("Waiting: (none)\n");
				else
					printf("Waiting:\n");
				for (did = 1, j = 0, q = p->waitersH; q; j++, q = q->next) {
					printf("%c(%ld-%ld", q->type, (long) q->start, (long) q->stop);
					if (q->start2 != -1)
						printf(",%ld-%ld) ", (long) q->start2, (long) q->stop2);
					else
						printf(") ");
					if (j && !(j % 4)) {
						printf("\n         ");
						did = 1;
					} else
						did = 0;
				}
				if (!did)
					printf("\n");
			}
		}
	if (!foundone)
		printf("(none)\n");
	else
		printf("\n");
	RF_UNLOCK_MUTEX(rf_printf_mutex);
}
