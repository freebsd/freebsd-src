#	$Id: Makefile,v 1.4 1997/02/22 12:47:46 peter Exp $

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_coff_mod
SRCS= 	coff.c imgact_coff.c vnode_if.h
NOMAN=
CLEANFILES+= vnode_if.h vnode_if.c
CFLAGS+= -DLKM -DCOMPAT_IBCS2

.include <bsd.kmod.mk>
