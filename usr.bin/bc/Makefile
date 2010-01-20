#	$FreeBSD$
#	$OpenBSD: Makefile,v 1.4 2006/06/30 19:02:28 otto Exp $

PROG=		bc
SRCS=		bc.y scan.l
CFLAGS+=	-I. -I${.CURDIR}
WARNS?=		6
#SUBDIR+=	USD.doc

FILES+=		bc.library
FILESDIR=	${SHAREDIR}/misc

#beforeinstall:
#	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
#	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
