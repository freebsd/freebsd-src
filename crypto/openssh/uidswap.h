/*	$OpenBSD: uidswap.h,v 1.7 2001/04/06 21:00:17 markus Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef UIDSWAP_H
#define UIDSWAP_H

/*
 * Temporarily changes to the given uid.  If the effective user id is not
 * root, this does nothing.  This call cannot be nested.
 */
void    temporarily_use_uid(struct passwd *pw);

/*
 * Restores the original effective user id after temporarily_use_uid().
 * This should only be called while temporarily_use_uid is effective.
 */
void    restore_uid(void);

/*
 * Permanently sets all uids to the given uid.  This cannot be called while
 * temporarily_use_uid is effective.  This must also clear any saved uids.
 */
void    permanently_set_uid(struct passwd *pw);

#endif				/* UIDSWAP_H */
