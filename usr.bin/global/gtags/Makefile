# $Id: Makefile,v 1.5 1997/07/30 04:55:34 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gtags

MAN1=	gtags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/gtags/gtags.pl ${DESTDIR}/usr/bin/gtags

.include <bsd.prog.mk>
