# $FreeBSD: src/usr.bin/global/systags/Makefile,v 1.5 1999/08/28 01:01:53 peter Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/systags

NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/systags/systags.sh ${DESTDIR}${BINDIR}/systags

.include <bsd.prog.mk>
