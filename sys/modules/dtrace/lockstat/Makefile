# $FreeBSD: src/sys/modules/dtrace/lockstat/Makefile,v 1.1.4.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/lockstat

KMOD=		lockstat
SRCS=		lockstat.c 

SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
