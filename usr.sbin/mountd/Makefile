#	From: @(#)Makefile	8.3 (Berkeley) 1/25/94
# $FreeBSD: src/usr.sbin/mountd/Makefile,v 1.17.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	mountd
SRCS=	mountd.c getmntopts.c
MAN=	exports.5 netgroup.5 mountd.8

MOUNT=  ${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
WARNS?= 2

.PATH: ${MOUNT}

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
