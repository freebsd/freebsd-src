# $FreeBSD: src/sys/modules/cyclic_test/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../cddl/dev/cyclic

KMOD=		cyclic_test
SRCS=		cyclic_test.c
		
#SRCS+=		bus_if.h device_if.h vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
