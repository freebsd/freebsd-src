/*
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)kvm_getvfsbyname.c	8.1 (Berkeley) 4/3/95";
#endif
static const char rcsid[] =
	"$Id: getvfsbyname.c,v 1.2 1997/03/03 13:08:33 bde Exp $";
#endif /* not lint */

#define	_NEW_VFSCONF
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <kvm.h>

/*
 * Given a filesystem name, determine if it is resident in the kernel,
 * and if it is resident, return its vfsconf structure.
 */
getvfsbyname(fsname, vfcp)
	const char *fsname;
	struct vfsconf *vfcp;
{
#ifdef	__NETBSD_SYSCALLS
	errno = ENOSYS;
#else
	int name[4], maxtypenum, cnt;
	size_t buflen;

	name[0] = CTL_VFS;
	name[1] = VFS_GENERIC;
	name[2] = VFS_MAXTYPENUM;
	buflen = 4;
	if (sysctl(name, 3, &maxtypenum, &buflen, (void *)0, (size_t)0) < 0)
		return (-1);
	name[2] = VFS_CONF;
	buflen = sizeof *vfcp;
	for (cnt = 0; cnt < maxtypenum; cnt++) {
		name[3] = cnt;
		if (sysctl(name, 4, vfcp, &buflen, (void *)0, (size_t)0) < 0) {
			if (errno != EOPNOTSUPP)
				return (-1);
			continue;
		}
		if (!strcmp(fsname, vfcp->vfc_name))
			return (0);
	}
	errno = ENOENT;
#endif
	return (-1);
}
