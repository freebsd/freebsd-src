#
#	$Id: Makefile,v 1.2.6.1 1998/03/06 08:06:39 jkh Exp $
#

PROG=	mount_msdos
SRCS=	mount_msdos.c getmntopts.c
MAN8=	mount_msdos.8

MOUNT=	${.CURDIR}/../../mount
CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../../sys
.PATH:	${MOUNT}

TABDIR= ${DESTDIR}/usr/libdata/msdosfs
TABLES= koi2dos

afterinstall:
	cd ${.CURDIR} && \
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	${TABLES} ${TABDIR}

.include <bsd.prog.mk>
