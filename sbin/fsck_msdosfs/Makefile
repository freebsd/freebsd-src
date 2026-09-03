#	$NetBSD: Makefile,v 1.6 1997/05/08 21:11:11 gwr Exp $

.include <src.opts.mk>

FSCK=	${.CURDIR:H}/fsck
.PATH: ${FSCK}

PACKAGE=runtime
PROG=	fsck_msdosfs
MAN=	fsck_msdosfs.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c

CFLAGS+= -I${FSCK} -DHAVE_LIBUTIL_H
LIBADD=	util

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
