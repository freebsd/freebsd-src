#
# $FreeBSD: src/sbin/mount_msdos/Makefile,v 1.14.2.1 2000/08/01 09:34:45 sheldonh Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

TABDIR= ${DESTDIR}${LIBDATADIR}/msdosfs
TABLES= iso22dos koi2dos

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${TABLES} ${TABDIR}

.include <bsd.prog.mk>
