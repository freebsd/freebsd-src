# $Id: Makefile,v 1.3 1997/12/15 23:34:55 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/htags

MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/htags/htags.pl ${DESTDIR}${BINDIR}/htags

.include <bsd.prog.mk>
