/* sigaddset.c: A replacement for the sigaddset function

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.2.
*/

#include "opie_cfg.h"

#ifndef _NSIG
#ifdef NSIG
#define _NSIG NSIG
#else /* NSIG */
#define _NSIG 32
#endif /* NSIG */
#endif /* _NSIG */

#include "opie.h"

int sigaddset FUNCTION((set, signum), sigset_t *set AND int signum)
{
#if sizeof(sigset_t) != sizeof(int)
Sorry, we don't currently support your system.
#else /* sizeof(sigset_t) != sizeof(int) */
	if (set && (signum > 0) && (signum < _NSIG))
		*set |= 1 << (signum - 1);
#endif /* sizeof(sigset_t) != sizeof(int) */

	return 0;
}
