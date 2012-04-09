# $FreeBSD: src/sys/modules/dtrace/lockstat/Makefile,v 1.1.4.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/dev/lockstat

KMOD=		lockstat
SRCS=		lockstat.c 

SRCS+=		vnode_if.h opt_kdtrace.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
