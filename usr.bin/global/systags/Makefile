# $FreeBSD$

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/systags

NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/systags/systags.sh ${DESTDIR}${BINDIR}/systags

.include <bsd.prog.mk>
