# $FreeBSD: src/usr.bin/global/htags/Makefile,v 1.4 1999/08/28 01:01:50 peter Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/htags

MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/htags/htags.pl ${DESTDIR}${BINDIR}/htags

.include <bsd.prog.mk>
