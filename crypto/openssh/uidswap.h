/*
 * 
 * uidswap.h
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Sat Sep  9 01:43:15 1995 ylo
 * Last modified: Sat Sep  9 02:34:04 1995 ylo
 * 
 */

#ifndef UIDSWAP_H
#define UIDSWAP_H

/*
 * Temporarily changes to the given uid.  If the effective user id is not
 * root, this does nothing.  This call cannot be nested.
 */
void    temporarily_use_uid(uid_t uid);

/*
 * Restores the original effective user id after temporarily_use_uid().
 * This should only be called while temporarily_use_uid is effective.
 */
void    restore_uid();

/*
 * Permanently sets all uids to the given uid.  This cannot be called while
 * temporarily_use_uid is effective.  This must also clear any saved uids.
 */
void    permanently_set_uid(uid_t uid);

#endif				/* UIDSWAP_H */
