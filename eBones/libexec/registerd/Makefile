#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# %sccs.include.redist.sh
#
#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#
# $FreeBSD$

PROG=	registerd
SRCS=	registerd.c
CFLAGS+=-DCRYPT -DKERBEROS -I${.CURDIR}/../../usr.bin/register 
DPADD+=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
MAN8=	registerd.8

.include <bsd.prog.mk>
