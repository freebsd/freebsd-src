/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 * $Id: lock.c,v 1.6 1998/07/28 06:32:57 grog Exp grog $
 */

#define REALLYKERNEL
#include "vinumhdr.h"

/* Lock routines.  Currently, we lock either an individual volume
 * or the global configuration.  I don't think tsleep and
 * wakeup are SMP safe. FIXME XXX */

/* Lock a volume, wait if it's in use */
int 
lockvol(struct volume *vol)
{
    int error;

    while ((vol->flags & VF_LOCKED) != 0) {
	vol->flags |= VF_LOCKING;
	/* It would seem to make more sense to sleep on
	 * the address 'vol'.  Unfortuntaly we can't
	 * guarantee that this address won't change due to
	 * table expansion.  The address we choose won't change. */
	if ((error = tsleep(&vinum_conf.volume + vol->devno,
		    PRIBIO | PCATCH,
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
	wakeup(&vinum_conf.volume + vol->devno);
    }
}

/* Lock a plex, wait if it's in use */
int 
lockplex(struct plex *plex)
{
    int error;

    while ((plex->flags & VF_LOCKED) != 0) {
	plex->flags |= VF_LOCKING;
	/* It would seem to make more sense to sleep on
	 * the address 'plex'.  Unfortuntaly we can't
	 * guarantee that this address won't change due to
	 * table expansion.  The address we choose won't change. */
	if ((error = tsleep(&vinum_conf.plex + plex->sdnos[0],
		    PRIBIO | PCATCH,
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


/* Get a lock for the global config, wait if it's not available */
int 
lock_config(void)
{
    int error;

    while ((vinum_conf.flags & VF_LOCKED) != 0) {
	vinum_conf.flags |= VF_LOCKING;
	if ((error = tsleep(&vinum_conf, PRIBIO | PCATCH, "vincfg", 0)) != 0)
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
