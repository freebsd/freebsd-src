/* $FreeBSD: src/cddl/compat/opensolaris/include/mnttab.h,v 1.2.2.3.2.1 2010/02/10 00:26:20 kensmith Exp $ */

#ifndef	_OPENSOLARIS_MNTTAB_H_
#define	_OPENSOLARIS_MNTTAB_H_

#include <stdio.h>
#include <paths.h>

#define	MNTTAB		_PATH_DEVNULL
#define	MNT_LINE_MAX	1024

#define	umount2(p, f)	unmount(p, f)

struct mnttab {
	char	*mnt_special;
	char	*mnt_mountp;
	char	*mnt_fstype;
	char	*mnt_mntopts;
};

int getmntany(FILE *fd, struct mnttab *mgetp, struct mnttab *mrefp);

#endif	/* !_OPENSOLARIS_MNTTAB_H_ */
