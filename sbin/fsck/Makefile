#	$NetBSD: Makefile,v 1.14 1996/09/27 22:38:37 christos Exp $
# $FreeBSD$

PACKAGE=runtime
PROG=	fsck
SRCS=	fsck.c fsutil.c preen.c
SRCS+=	getmntopts.c
MAN=	fsck.8
MOUNT=	${SRCTOP}/sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:  ${MOUNT}

.include <bsd.prog.mk>
