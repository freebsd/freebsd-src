/* initgroups.c: Replacement for the initgroups() function.

%%% portions-copyright-cmetz
Portions of this software are Copyright 1996 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

        Modified by cmetz for OPIE 2.2. Removed useless string.
              Ifdef around headers. Use FUNCTION declarations.
              Not everyone has multiple groups. Work around
              lack of NGROUPS.
        Originally from 4.3BSD Net/2.
*/
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * initgroups
 */
#include "opie_cfg.h"

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <grp.h>

#include "opie.h"

struct group *getgrent();

int initgroups FUNCTION((uname, agroup), const char *uname AND int agroup)
{
#if HAVE_SETGROUPS && HAVE_GETGROUPS
#if NGROUPS
	int groups[NGROUPS];
#else /* NGROUPS */
#define STARTING_NGROUPS 32
	int groups[STARTING_NGROUPS];
#endif /* NGROUPS */
        int ngroups;
	register struct group *grp;
	register int i;

	/*
	 * If installing primary group, duplicate it;
	 * the first element of groups is the effective gid
	 * and will be overwritten when a setgid file is executed.
	 */
	if (agroup >= 0) {
		groups[ngroups++] = agroup;
		groups[ngroups++] = agroup;
	}
	setgrent();
	while (grp = getgrent()) {
		if (grp->gr_gid == agroup)
			continue;
		for (i = 0; grp->gr_mem[i]; i++)
			if (!strcmp(grp->gr_mem[i], uname)) {
#if NGROUPS
				if (ngroups == NGROUPS) {
#else /* NGROUPS */
                                if (ngroups == STARTING_NGROUPS) {
#endif /* NGROUPS */
fprintf(stderr, "initgroups: %s is in too many groups\n", uname);
					goto toomany;
				}
				groups[ngroups++] = grp->gr_gid;
			}
	}
toomany:
	endgrent();
#if NGROUPS
	if (setgroups(ngroups, groups) < 0) {
		perror("setgroups");
		return (-1);
	}
#else /* NGROUPS */
        ngroups++;
        do { 
          if ((i = setgroups(--ngroups, groups) < 0) && (i != EINVAL)) {
		perror("setgroups");
		return (-1);
          }
	} while ((i < 0) && (ngroups > 0));
#endif /* NGROUPS */
#endif /* HAVE_SETGROUPS && HAVE_GETGROUPS */
	return (0);
}
