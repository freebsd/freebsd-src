#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# %sccs.include.redist.sh
#
#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#
# $Id: Makefile,v 1.4 1995/09/13 17:24:03 markm Exp $

PROG=	registerd
SRCS=	registerd.c
CFLAGS+=-DCRYPT -DKERBEROS -I${.CURDIR}/../../usr.bin/register 
DPADD+=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN8=	registerd.8

.include <bsd.prog.mk>
