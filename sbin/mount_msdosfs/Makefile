#
# $FreeBSD$
#

PROG=	mount_msdosfs
SRCS=	mount_msdosfs.c getmntopts.c
MAN=	mount_msdosfs.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

TABDIR= ${DESTDIR}${LIBDATADIR}/msdosfs
TABLES= iso22dos iso72dos koi2dos koi8u2dos

afterinstall:
	cd ${.CURDIR} && \
	    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	    ${TABLES} ${TABDIR}

.include <bsd.prog.mk>
