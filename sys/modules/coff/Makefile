# $FreeBSD: src/sys/modules/coff/Makefile,v 1.15 2002/01/11 15:48:38 ru Exp $

.PATH: ${.CURDIR}/../../i386/ibcs2

KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h

CFLAGS+=	-DCOMPAT_IBCS2

.include <bsd.kmod.mk>
