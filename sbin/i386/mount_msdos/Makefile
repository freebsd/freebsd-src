#
# $FreeBSD$
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

MOUNT=	${.CURDIR}/../../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

TABDIR= ${DESTDIR}/usr/libdata/msdosfs
TABLES= koi2dos

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${TABLES} ${TABDIR}

.include <bsd.prog.mk>
