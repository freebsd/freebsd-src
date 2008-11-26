# $FreeBSD: src/sys/modules/coff/Makefile,v 1.15.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../i386/ibcs2

KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h

CFLAGS+=	-DCOMPAT_IBCS2

.include <bsd.kmod.mk>
