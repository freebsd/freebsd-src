#
# $FreeBSD: src/sbin/mount_msdos/Makefile,v 1.14.2.4 2002/04/14 16:57:18 phantom Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN=	mount_msdos.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

TABDIR= ${DESTDIR}${LIBDATADIR}/msdosfs
TABLES= iso22dos iso72dos koi2dos koi8u2dos

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${TABLES} ${TABDIR}

.include <bsd.prog.mk>
