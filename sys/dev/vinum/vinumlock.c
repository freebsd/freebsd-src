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
 * $Id: vinumlock.c,v 1.13 2000/05/02 23:25:02 grog Exp grog $
 * $FreeBSD: src/sys/dev/vinum/vinumlock.c,v 1.18.2.1 2000/05/11 08:49:22 grog Exp $
 */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/request.h>

/*
 * Lock routines.  Currently, we lock either an individual volume
 * or the global configuration.  I don't think tsleep and
 * wakeup are SMP safe. FIXME XXX
 */

/* Lock a drive, wait if it's in use */
#if VINUMDEBUG
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

/* Lock a volume, wait if it's in use */
int
lockvol(struct volume *vol)
{
    int error;

    while ((vol->flags & VF_LOCKED) != 0) {
	vol->flags |= VF_LOCKING;
	/*
	 * It would seem to make more sense to sleep on
	 * the address 'vol'.  Unfortuntaly we can't
	 * guarantee that this address won't change due to
	 * table expansion.  The address we choose won't change.
	 */
	if ((error = tsleep(&vinum_conf.volume + vol->volno,
		    PRIBIO,
		    "volock",
		    0)) != 0)
	    return error;
    }
    vol->flags |= VF_LOCKED;
    return 0;
}

/* Unlock a volume and let the next one at it */
void
unlockvol(struct volume *vol)
{
    vol->flags &= ~VF_LOCKED;
    if ((vol->flags & VF_LOCKING) != 0) {
	vol->flags &= ~VF_LOCKING;
	wakeup(&vinum_conf.volume + vol->volno);
    }
}

/* Lock a plex, wait if it's in use */
int
lockplex(struct plex *plex)
{
    int error;

    while ((plex->flags & VF_LOCKED) != 0) {
	plex->flags |= VF_LOCKING;
	/*
	 * It would seem to make more sense to sleep on
	 * the address 'plex'.  Unfortunately we can't
	 * guarantee that this address won't change due to
	 * table expansion.  The address we choose won't change.
	 */
	if ((error = tsleep(&vinum_conf.plex + plex->sdnos[0],
		    PRIBIO,
		    "plexlk",
		    0)) != 0)
	    return error;
    }
    plex->flags |= VF_LOCKED;
    return 0;
}

/* Unlock a plex and let the next one at it */
void
unlockplex(struct plex *plex)
{
    plex->flags &= ~VF_LOCKED;
    if ((plex->flags & VF_LOCKING) != 0) {
	plex->flags &= ~VF_LOCKING;
	wakeup(&vinum_conf.plex + plex->plexno);
    }
}

/* Lock a stripe of a plex, wait if it's in use */
struct rangelock *
lockrange(daddr_t stripe, struct buf *bp, struct plex *plex)
{
    int s;
    struct rangelock *lock;
    struct rangelock *pos;				    /* position of first free lock */
    int foundlocks;					    /* number of locks found */
    int newlock;

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
    /*
     * We give the locks back from an interrupt
     * context, so we need to raise the spl here.
     */
    s = splbio();

    /* Search the lock table for our stripe */
    for (lock = plex->lock;
	lock < &plex->lock[plex->alloclocks]
	&& foundlocks < plex->usedlocks;
	lock++) {
	if (lock->stripe) {				    /* in use */
	    foundlocks++;				    /* found another one in use */
	    if ((lock->stripe == stripe)		    /* it's our stripe */
&&(lock->plexno == plex->plexno)			    /* and our plex */
	    &&(lock->bp != bp)) {			    /* but not our request */
		/*
		 * It would be nice to sleep on the lock
		 * itself, but it could get moved if the
		 * table expands during the wait.  Wait on
		 * the lock address + 1 (since waiting on
		 * 0 isn't allowed) instead.  It isn't
		 * exactly unique, but we won't have many
		 * conflicts.  The worst effect of a
		 * conflict would be an additional
		 * schedule and time through this loop.
		 */
#ifdef VINUMDEBUG
		if (debug & DEBUG_LASTREQS) {
		    struct rangelock info;

		    info.stripe = stripe;
		    info.bp = bp;
		    info.plexno = plex->plexno;
		    logrq(loginfo_lockwait, (union rqinfou) &info, bp);
		}
#endif
		plex->lockwaits++;			    /* waited one more time */
		tsleep((void *) lock->stripe, PRIBIO, "vrlock", 2 * hz);
		lock = plex->lock;			    /* start again */
		foundlocks = 0;
		pos = NULL;
	    }
	} else if (pos == NULL)				    /* still looking for somewhere? */
	    pos = lock;					    /* a place to put this one */
    }

    /*
     * The address range is free.  Add our lock
     * entry.
     */
    if (pos == NULL) {					    /* Didn't find an entry */
	if (foundlocks >= plex->alloclocks) {		    /* searched the lot, */
	    newlock = plex->alloclocks;
	    EXPAND(plex->lock, struct rangelock, plex->alloclocks, INITIAL_LOCKS);
	    pos = &plex->lock[newlock];
	    while (newlock < plex->alloclocks)
		plex->lock[newlock++].stripe = 0;
	} else
	    pos = lock;					    /* put it at the end */
    }
    pos->stripe = stripe;
    pos->bp = bp;
    pos->plexno = plex->plexno;
    plex->usedlocks++;					    /* one more lock */
    splx(s);
#ifdef VINUMDEBUG
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_lock, (union rqinfou) pos, bp);
#endif
    return pos;
}

/* Unlock a volume and let the next one at it */
void
unlockrange(int plexno, struct rangelock *lock)
{
    daddr_t lockaddr;

#ifdef VINUMDEBUG
    if (debug & DEBUG_LASTREQS)
	logrq(loginfo_unlock, (union rqinfou) lock, lock->bp);
#endif
    lockaddr = lock->stripe;
    lock->stripe = 0;					    /* no longer used */
    PLEX[plexno].usedlocks--;				    /* one less lock */
    wakeup((void *) lockaddr);
}

/* Get a lock for the global config, wait if it's not available */
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

/* Unlock and wake up any waiters  */
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
