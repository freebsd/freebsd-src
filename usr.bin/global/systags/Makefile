# $Id: Makefile,v 1.3 1997/12/15 02:28:24 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/systags

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/systags/systags.sh ${DESTDIR}${BINDIR}/systags

.include <bsd.prog.mk>
