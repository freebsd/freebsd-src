/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
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
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumlock.c,v 1.19 2003/05/23 01:07:18 grog Exp $
 * $FreeBSD$
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

/* Lock a drive, wait if it's in use */
#ifdef VINUMDEBUG
int
lockdrive(struct drive *drive, char *file, int line)
#else
int
lockdrive(struct drive *drive)
#endif
{
    int error;

    /* XXX get rid of     drive->flags |= VF_LOCKING; */
    if ((drive->flags & VF_LOCKED)			    /* it's locked */
    &&(drive->pid == curproc->p_pid)) {			    /* by us! */
#ifdef VINUMDEBUG
	log(LOG_WARNING,
	    "vinum lockdrive: already locking %s from %s:%d, called from %s:%d\n",
	    drive->label.name,
	    drive->lockfilename,
	    drive->lockline,
	    basename(file),
	    line);
#else
	log(LOG_WARNING,
	    "vinum lockdrive: already locking %s\n",
	    drive->label.name);
#endif
	return 0;
    }
    while ((drive->flags & VF_LOCKED) != 0) {
	/*
	 * There are problems sleeping on a unique identifier,
	 * since the drive structure can move, and the unlock
	 * function can be called after killing the drive.
	 * Solve this by waiting on this function; the number
	 * of conflicts is negligible.
	 */
	if ((error = tsleep(&lockdrive,
		    PRIBIO,
		    "vindrv",
		    0)) != 0)
	    return error;
    }
    drive->flags |= VF_LOCKED;
    drive->pid = curproc->p_pid;			    /* it's a panic error if curproc is null */
#ifdef VINUMDEBUG
    bcopy(basename(file), drive->lockfilename, 15);
    drive->lockfilename[15] = '\0';			    /* truncate if necessary */
    drive->lockline = line;
#endif
    return 0;
}

/* Unlock a drive and let the next one at it */
void
unlockdrive(struct drive *drive)
{
    drive->flags &= ~VF_LOCKED;
    /* we don't reset pid: it's of hysterical interest */
    wakeup(&lockdrive);
}

/* Lock a stripe of a plex, wait if it's in use */
struct rangelock *
lockrange(daddr_t stripe, struct buf *bp, struct plex *plex)
{
    struct rangelock *lock;
    struct rangelock *pos;				    /* position of first free lock */
    int foundlocks;					    /* number of locks found */

    /*
     * We could get by without counting the number
     * of locks we find, but we have a linear search
     * through a table which in most cases will be
     * empty.  It's faster to stop when we've found
     * all the locks that are there.  This is also
     * the reason why we put pos at the beginning
     * instead of the end, though it requires an
     * extra test.
     */
    pos = NULL;
    foundlocks = 0;

    /*
     * we can't use 0 as a valid address, so
     * increment all addresses by 1.
     */
    stripe++;
    mtx_lock(plex->lockmtx);

    /* Wait here if the table is full */
    while (plex->usedlocks == PLEX_LOCKS)		    /* all in use */
	msleep(&plex->usedlocks, plex->lockmtx, PRIBIO, "vlock", 0);

#ifdef DIAGNOSTIC
    if (plex->usedlocks >= PLEX_LOCKS)
	panic("lockrange: Too many locks in use");
#endif

    lock = plex->lock;					    /* pointer in lock table */
    if (plex->usedlocks > 0)				    /* something locked, */
	/* Search the lock table for our stripe */
	for (; lock < &plex->lock[PLEX_LOCKS]
	    && foundlocks < plex->usedlocks;
	    lock++) {
	    if (lock->stripe) {				    /* in use */
		foundlocks++;				    /* found another one in use */
		if ((lock->stripe == stripe)		    /* it's our stripe */
		&&(lock->bp != bp)) {			    /* but not our request */
#ifdef VINUMDEBUG
		    if (debug & DEBUG_LOCKREQS) {
			struct rangelockinfo lockinfo;

			lockinfo.stripe = stripe;
			lockinfo.bp = bp;
			lockinfo.plexno = plex->plexno;
			logrq(loginfo_lockwait, (union rqinfou) &lockinfo, bp);
		    }
#endif
		    plex->lockwaits++;			    /* waited one more time */
		    msleep(lock, plex->lockmtx, PRIBIO, "vrlock", 0);
		    lock = &plex->lock[-1];		    /* start again */
		    foundlocks = 0;
		    pos = NULL;
		}
	    } else if (pos == NULL)			    /* still looking for somewhere? */
		pos = lock;				    /* a place to put this one */
	}
    /*
     * This untidy looking code ensures that we'll
     * always end up pointing to the first free lock
     * entry, thus minimizing the number of
     * iterations necessary.
     */
    if (pos == NULL)					    /* didn't find one on the way, */
	pos = lock;					    /* use the one we're pointing to */

    /*
     * The address range is free, and we're pointing
     * to the first unused entry.  Make it ours.
     */
    pos->stripe = stripe;
    pos->bp = bp;
    plex->usedlocks++;					    /* one more lock */
    mtx_unlock(plex->lockmtx);
#ifdef VINUMDEBUG
    if (debug & DEBUG_LOCKREQS) {
	struct rangelockinfo lockinfo;

	lockinfo.stripe = stripe;
	lockinfo.bp = bp;
	lockinfo.plexno = plex->plexno;
	logrq(loginfo_lock, (union rqinfou) &lockinfo, bp);
    }
#endif
    return pos;
}

/* Unlock a volume and let the next one at it */
void
unlockrange(int plexno, struct rangelock *lock)
{
    struct plex *plex;

    plex = &PLEX[plexno];
#ifdef DIAGNOSTIC
    if (lock < &plex->lock[0] || lock >= &plex->lock[PLEX_LOCKS])
	panic("vinum: rangelock %p on plex %d invalid, not between %p and %p",
	    lock,
	    plexno,
	    &plex->lock[0],
	    &plex->lock[PLEX_LOCKS]);
#endif
#ifdef VINUMDEBUG
    if (debug & DEBUG_LOCKREQS) {
	struct rangelockinfo lockinfo;

	lockinfo.stripe = lock->stripe;
	lockinfo.bp = lock->bp;
	lockinfo.plexno = plex->plexno;
	logrq(loginfo_lockwait, (union rqinfou) &lockinfo, lock->bp);
    }
#endif
    lock->stripe = 0;					    /* no longer used */
    plex->usedlocks--;					    /* one less lock */
    if (plex->usedlocks == PLEX_LOCKS - 1)		    /* we were full, */
	wakeup(&plex->usedlocks);			    /* get a waiter if one's there */
    wakeup((void *) lock);
}

/* Get a lock for the global config.  Wait if it's not available. */
int
lock_config(void)
{
    int error;

    while ((vinum_conf.flags & VF_LOCKED) != 0) {
	vinum_conf.flags |= VF_LOCKING;
	if ((error = tsleep(&vinum_conf, PRIBIO, "vincfg", 0)) != 0)
	    return error;
    }
    vinum_conf.flags |= VF_LOCKED;
    return 0;
}

/* Unlock global config and wake up any waiters. */
void
unlock_config(void)
{
    vinum_conf.flags &= ~VF_LOCKED;
    if ((vinum_conf.flags & VF_LOCKING) != 0) {
	vinum_conf.flags &= ~VF_LOCKING;
	wakeup(&vinum_conf);
    }
}
/* Local Variables: */
/* fill-column: 50 */
/* End: */
