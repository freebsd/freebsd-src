# $FreeBSD: src/sbin/mount_udf/Makefile,v 1.4 2004/02/23 20:13:54 johan Exp $

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys -Wall
.PATH:	${MOUNT}
WARNS?= 1

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NOSHARED?=	NO

.include <bsd.prog.mk>
