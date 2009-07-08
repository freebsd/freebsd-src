#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	fstat
SRCS=	cd9660.c common.c fstat.c main.c msdosfs.c fuser.c
LINKS=	${BINDIR}/fstat ${BINDIR}/fuser
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555
WARNS?=	6

MAN1=	fuser.1 fstat.1

CFLAGS+=-D_KVM_VNODE

# XXX This is a hack.
.if ${MK_CDDL} != "no"
CFLAGS+= -DZFS
OBJS+=	zfs/zfs.o
SUBDIR=	zfs
zfs/zfs.o: .PHONY
	@cd ${.CURDIR}/zfs && ${MAKE} zfs.o
.endif

.include <bsd.prog.mk>
