/*********************************************************************
 *                
 * Filename:      irmod.h
 * Version:       0.3
 * Description:   IrDA module and utilities functions
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Dec 15 13:58:52 1997
 * Modified at:   Fri Jan 28 13:15:24 2000
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 *
 *     Copyright (c) 1998-2000 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charg.
 *     
 ********************************************************************/

#ifndef IRMOD_H
#define IRMOD_H

#include <net/irda/irda.h>		/* Notify stuff */

/* Nothing much here anymore - Maybe this header should be merged in
 * another header like net/irda/irda.h... - Jean II */

/* Locking wrapper - Note the inverted logic on irda_lock().
 * Those function basically return false if the lock is already in the
 * position you want to set it. - Jean II */
#define irda_lock(lock)		(! test_and_set_bit(0, (void *) (lock)))
#define irda_unlock(lock)	(test_and_clear_bit(0, (void *) (lock)))

/* Zero the notify structure */
void irda_notify_init(notify_t *notify);

#endif /* IRMOD_H */









