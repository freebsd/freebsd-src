/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 * Contributed by Exactis.com, Inc.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: shmticklib.c,v 8.6 2000/02/26 01:32:27 gshapiro Exp $";
#endif /* ! lint */

#if _FFR_SHM_STATUS
# if SFIO
#  include <sfio/stdio.h>
# else /* !SFIO */
#  include <stdio.h>
# endif /* SFIO */
# include <sys/types.h>
# include <sys/ipc.h>
# include <sys/shm.h>

# include "statusd_shm.h"

/*
**  SHMTICK -- increment a shared memory variable
**
**	Parameters:
**		inc_me -- identity of shared memory segment
**		what -- which variable to increment
**
**	Returns:
**		none
*/

void
shmtick(inc_me, what)
	int inc_me;
	int what;
{
	static int shmid = -1;
	static STATUSD_SHM *sp = (STATUSD_SHM *)-1;
	static unsigned int cookie = 0;

	if (shmid < 0)
	{
		int size = sizeof(STATUSD_SHM);

		shmid = shmget(STATUSD_SHM_KEY, size, 0);
		if (shmid < 0)
			return;
	}
	if ((unsigned long *)sp == (unsigned long *)-1)
	{
		sp = (STATUSD_SHM *)shmat(shmid, NULL, 0);
		if ((unsigned long *)sp == (unsigned long *)-1)
			return;
	}
	if (sp->magic != STATUSD_MAGIC)
	{
		/*
		**  possible race condition, wait for
		**  statusd to initialize.
		*/

		return;
	}
	if (what >= STATUSD_LONGS)
		what = STATUSD_LONGS - 1;
	if (inc_me >= STATUSD_LONGS)
		inc_me = STATUSD_LONGS - 1;

	if (sp->ul[STATUSD_COOKIE] != cookie)
	{
		cookie = sp->ul[STATUSD_COOKIE];
		++(sp->ul[inc_me]);
	}
	++(sp->ul[what]);
}
#endif /* _FFR_SHM_STATUS */
