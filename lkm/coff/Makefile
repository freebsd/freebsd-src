#	$Id: Makefile,v 1.1 1994/10/14 08:46:11 sos Exp $

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_coff_mod
SRCS= 	coff.c imgact_coff.c vnode_if.h
NOMAN=
CLEANFILES+= vnode_if.h vnode_if.c
CFLAGS+= -DLKM -I. -DCOMPAT_IBCS2

.include <bsd.kmod.mk>
