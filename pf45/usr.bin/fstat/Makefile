#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	fstat
SRCS=	cd9660.c fstat.c msdosfs.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

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
