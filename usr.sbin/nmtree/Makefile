#       $NetBSD: Makefile,v 1.32 2009/04/22 15:23:05 lukem Exp $
#       from: @(#)Makefile      8.2 (Berkeley) 4/27/95

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/mtree

PROG=   nmtree
CFLAGS+=	-DMTREE
CFLAGS+=	-DNO_SHA384 -I${.CURDIR}
LDADD+=		-lmd -lutil
#MAN=    mtree.8
MAN=
SRCS=   compare.c crc.c create.c excludes.c misc.c mtree.c spec.c verify.c \
        getid.c pack_dev.c

SRCS+=	sha1.h sha2.h util.c util.h

CFLAGS+=	-I${.CURDIR}/../../contrib/mknod
.PATH:		${.CURDIR}/../../contrib/mknod

.include <bsd.prog.mk>
