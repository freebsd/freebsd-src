# $FreeBSD$

.PATH:	${.CURDIR}/../../i386/ibcs2
KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h
NOMAN=
CFLAGS+=	-DCOMPAT_IBCS2
KMODDEPS=	ibcs2

.include <bsd.kmod.mk>
