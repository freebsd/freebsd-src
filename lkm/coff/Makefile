#	$Id: Makefile,v 1.5 1998/02/01 18:12:12 bde Exp $

.PATH:	${.CURDIR}/../../sys/i386/ibcs2
KMOD=	ibcs2_coff_mod
SRCS= 	coff.c imgact_coff.c vnode_if.h
NOMAN=
CLEANFILES+= vnode_if.h vnode_if.c
CFLAGS+= -DLKM -DCOMPAT_IBCS2
DEPLKMS=${.OBJDIR}/../ibcs2/ibcs2_mod.o

.include <bsd.kmod.mk>
