# $FreeBSD: src/sbin/mount_udf/Makefile,v 1.3 2003/11/05 06:21:45 scottl Exp $

PROG=	mount_udf
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys -Wall
.PATH:	${MOUNT}
WARNS= 1

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NOSHARED?=	NO

.include <bsd.prog.mk>
