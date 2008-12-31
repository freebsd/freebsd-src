/* $FreeBSD: src/cddl/compat/opensolaris/include/mnttab.h,v 1.2.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $ */

#ifndef	_OPENSOLARIS_MNTTAB_H_
#define	_OPENSOLARIS_MNTTAB_H_

#include <stdio.h>
#include <paths.h>

#define	MNTTAB		_PATH_DEVNULL
#define	MNT_LINE_MAX	1024

struct mnttab {
	char	*mnt_special;
	char	*mnt_mountp;
	char	*mnt_fstype;
	char	*mnt_mntopts;
};

int getmntany(FILE *fd, struct mnttab *mgetp, struct mnttab *mrefp);

#endif	/* !_OPENSOLARIS_MNTTAB_H_ */
