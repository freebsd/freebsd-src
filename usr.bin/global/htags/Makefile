# $Id: Makefile,v 1.3 1997/07/30 04:52:59 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/htags

MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/htags/htags.pl ${DESTDIR}/usr/bin/htags

.include <bsd.prog.mk>
