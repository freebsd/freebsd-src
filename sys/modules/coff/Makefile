# $FreeBSD: src/sys/modules/coff/Makefile,v 1.15.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../i386/ibcs2

KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h

CFLAGS+=	-DCOMPAT_IBCS2

.include <bsd.kmod.mk>
