# $FreeBSD: src/usr.bin/global/htags/Makefile,v 1.3.2.1 1999/08/29 15:28:31 peter Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/htags

MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/htags/htags.pl ${DESTDIR}${BINDIR}/htags

.include <bsd.prog.mk>
