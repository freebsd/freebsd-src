#       $NetBSD: Makefile,v 1.32 2009/04/22 15:23:05 lukem Exp $
#       from: @(#)Makefile      8.2 (Berkeley) 4/27/95

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/mtree

PROG=   nmtree
CFLAGS+=	-DMTREE
CFLAGS+=	-DNO_SHA384 -I${.CURDIR}
LDADD+=		-lmd -lutil
MAN=    nmtree.8
SRCS=   compare.c crc.c create.c excludes.c misc.c mtree.c spec.c specspec.c \
	verify.c \
        getid.c pack_dev.c

CFLAGS+=	-I${.CURDIR}/../../lib/libnetbsd

CFLAGS+=	-I${.CURDIR}/../../contrib/mknod
.PATH:		${.CURDIR}/../../contrib/mknod

LIBNETBSDDIR=   ${.OBJDIR}/../../lib/libnetbsd
LIBNETBSD=      ${LIBNETBSDDIR}/libnetbsd.a
DPADD+=         ${LIBNETBSD}
LDADD+=         ${LIBNETBSD}

nmtree.8: mtree.8
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
