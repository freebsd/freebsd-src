#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# %sccs.include.redist.sh
#
#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#
# $Id: Makefile,v 1.4 1995/07/18 16:41:24 mark Exp $

PROG=	registerd
SRCS=	registerd.c
CFLAGS+=-DCRYPT -DKERBEROS -I${.CURDIR}/../register
.PATH:	${.CURDIR}/../../usr.bin/rlogin
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes
MAN8=	registerd.8
BINDIR=	/usr/libexec

.include <bsd.prog.mk>
