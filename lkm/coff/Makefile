#	$Id: Makefile,v 1.6 1998/03/12 20:04:38 eivind Exp $

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_coff_mod
SRCS= 	coff.c imgact_coff.c vnode_if.h
NOMAN=
CLEANFILES+= vnode_if.h vnode_if.c
CFLAGS+= -DLKM -DCOMPAT_IBCS2
DEPLKMS=${.OBJDIR}/../ibcs2/ibcs2_mod.o

.include <bsd.kmod.mk>
