/* uname.c: A replacement for the uname function (sort of)

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Modified by cmetz for OPIE 2.3. Ifdef around gethostname().
	Created by cmetz for OPIE 2.2.
*/
#include "opie_cfg.h"
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#include "opie.h"

int uname FUNCTION(struct utsname *buf)
{
#if HAVE_GETHOSTNAME
	char hostname[MAXHOSTNAMELEN], *c;

	memset(buf, 0, sizeof(buf));

	if (gethostname(hostname, sizeof(hostname)-1) < 0)
		return -1;

	hostname[sizeof(hostname) - 1] = 0;

	if (c = strchr(hostname, '.')) {
		*c = 0;
	}

	strncpy(buf->nodename, hostname, sizeof(buf->nodename) - 1);
	return 0;
#else /* HAVE_GETHOSTNAME */
	strncpy(buf->nodename, "unknown", sizeof(buf->nodename) - 1);
	return 0;
#endif /* HAVE_GETHOSTNAME */
}
