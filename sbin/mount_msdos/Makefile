#
# $FreeBSD: src/sbin/mount_msdos/Makefile,v 1.14.2.6 2002/08/07 16:31:48 ru Exp $
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
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${TABLES} ${TABDIR}

.include <bsd.prog.mk>
