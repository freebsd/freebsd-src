/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: shm.h,v 1.7 2001/04/20 15:21:55 ca Exp $
 */

#ifndef SM_SHM_H
# define SM_SHM_H

# if SM_CONF_SHM
#  include <sys/types.h>
#  include <sys/ipc.h>
#  include <sys/shm.h>

/* # include "def.h"	*/

/* key for shared memory */
#  define SM_SHM_KEY	((key_t) 42)

/* return value for failed shmget() */
#  define SM_SHM_NULL	((void *) -1)
#  define SM_SHM_NO_ID	(-1)
#  define SM_NO_SHM(id)	((id) < 0)

extern void *sm_shmstart __P((key_t, int , int , int *, bool));
extern int sm_shmstop __P((void *, int, bool));

/* for those braindead systems... (e.g., SunOS 4) */
#  ifndef SHM_R
#   define SHM_R       0400
#  endif /* SHM_R */
#  ifndef SHM_W
#   define SHM_W       0200
#  endif /* SHM_W */

# endif /* SM_CONF_SHM */
#endif /* ! SM_SHM_H */
