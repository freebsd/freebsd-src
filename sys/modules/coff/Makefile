# $FreeBSD: src/sys/modules/coff/Makefile,v 1.15.40.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../i386/ibcs2

KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h

CFLAGS+=	-DCOMPAT_IBCS2

.include <bsd.kmod.mk>
