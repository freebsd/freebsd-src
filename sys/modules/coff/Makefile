# $FreeBSD: src/sys/modules/coff/Makefile,v 1.15.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../i386/ibcs2

KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h

CFLAGS+=	-DCOMPAT_IBCS2

.include <bsd.kmod.mk>
