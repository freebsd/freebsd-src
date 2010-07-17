# $FreeBSD: src/sbin/mount_udf/Makefile,v 1.6.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys
.PATH:	${MOUNT}
WARNS?= 1

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.include <bsd.prog.mk>
