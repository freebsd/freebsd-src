# $FreeBSD: src/sys/modules/coff/Makefile,v 1.8.2.1 1999/08/29 16:27:09 peter Exp $

.PATH:	${.CURDIR}/../../i386/ibcs2
KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h
NOMAN=
CLEANFILES+=	vnode_if.h vnode_if.c
CFLAGS+=	-DCOMPAT_IBCS2
KMODDEPS=	ibcs2

.include <bsd.kmod.mk>
