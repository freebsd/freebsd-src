# $Id: Makefile,v 1.1.1.1.2.2 1998/07/17 04:18:13 jkh Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/systags

NOOBJ=

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/systags/systags.sh ${DESTDIR}${BINDIR}/systags

.include <bsd.prog.mk>
