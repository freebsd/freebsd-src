#	$Id: Makefile,v 1.7 1998/10/16 04:30:42 peter Exp $

.PATH:	${.CURDIR}/../../i386/ibcs2
KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h
NOMAN=
CLEANFILES+=	vnode_if.h vnode_if.c
CFLAGS+=	-DCOMPAT_IBCS2
KMODDEPS=	ibcs2

.include <bsd.kmod.mk>
