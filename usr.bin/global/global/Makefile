# $Id: Makefile,v 1.3 1997/07/30 04:40:51 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/global

MAN1=	global.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/global/global.pl ${DESTDIR}/usr/bin/global

.include <bsd.prog.mk>
