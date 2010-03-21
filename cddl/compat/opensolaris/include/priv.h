/* $FreeBSD: src/cddl/compat/opensolaris/include/priv.h,v 1.2.2.2.6.1 2010/02/10 00:26:20 kensmith Exp $ */

#ifndef	_OPENSOLARIS_PRIV_H_
#define	_OPENSOLARIS_PRIV_H_

#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#define	PRIV_SYS_CONFIG	0

static __inline int
priv_ineffect(priv)
{

	assert(priv == PRIV_SYS_CONFIG);
	return (geteuid() == 0);
}

#endif	/* !_OPENSOLARIS_PRIV_H_ */
