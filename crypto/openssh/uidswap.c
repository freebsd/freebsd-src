/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code for uid-swapping.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: uidswap.c,v 1.9 2000/09/07 20:27:55 deraadt Exp $");

#include "ssh.h"
#include "uidswap.h"

/*
 * Note: all these functions must work in all of the following cases:
 *    1. euid=0, ruid=0
 *    2. euid=0, ruid!=0
 *    3. euid!=0, ruid!=0
 * Additionally, they must work regardless of whether the system has
 * POSIX saved uids or not.
 */

#ifdef _POSIX_SAVED_IDS
/* Lets assume that posix saved ids also work with seteuid, even though that
   is not part of the posix specification. */
#define SAVED_IDS_WORK_WITH_SETEUID
#endif /* _POSIX_SAVED_IDS */

/* Saved effective uid. */
static uid_t saved_euid = 0;

/*
 * Temporarily changes to the given uid.  If the effective user
 * id is not root, this does nothing.  This call cannot be nested.
 */
void
temporarily_use_uid(uid_t uid)
{
#ifdef SAVED_IDS_WORK_WITH_SETEUID
	/* Save the current euid. */
	saved_euid = geteuid();

	/* Set the effective uid to the given (unprivileged) uid. */
	if (seteuid(uid) == -1)
		debug("seteuid %u: %.100s", (u_int) uid, strerror(errno));
#else /* SAVED_IDS_WORK_WITH_SETUID */
	/* Propagate the privileged uid to all of our uids. */
	if (setuid(geteuid()) < 0)
		debug("setuid %u: %.100s", (u_int) geteuid(), strerror(errno));

	/* Set the effective uid to the given (unprivileged) uid. */
	if (seteuid(uid) == -1)
		debug("seteuid %u: %.100s", (u_int) uid, strerror(errno));
#endif /* SAVED_IDS_WORK_WITH_SETEUID */
}

/*
 * Restores to the original uid.
 */
void
restore_uid()
{
#ifdef SAVED_IDS_WORK_WITH_SETEUID
	/* Set the effective uid back to the saved uid. */
	if (seteuid(saved_euid) < 0)
		debug("seteuid %u: %.100s", (u_int) saved_euid, strerror(errno));
#else /* SAVED_IDS_WORK_WITH_SETEUID */
	/*
	 * We are unable to restore the real uid to its unprivileged value.
	 * Propagate the real uid (usually more privileged) to effective uid
	 * as well.
	 */
	setuid(getuid());
#endif /* SAVED_IDS_WORK_WITH_SETEUID */
}

/*
 * Permanently sets all uids to the given uid.  This cannot be
 * called while temporarily_use_uid is effective.
 */
void
permanently_set_uid(uid_t uid)
{
	if (setuid(uid) < 0)
		debug("setuid %u: %.100s", (u_int) uid, strerror(errno));
}
