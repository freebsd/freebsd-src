# $FreeBSD: src/sys/modules/coff/Makefile,v 1.10 1999/11/28 18:52:45 bde Exp $

.PATH:	${.CURDIR}/../../i386/ibcs2
KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h
NOMAN=
CFLAGS+=	-DCOMPAT_IBCS2
KMODDEPS=	ibcs2

.include <bsd.kmod.mk>
