# $Id: Makefile,v 1.1.1.1.2.1 1998/02/05 04:53:29 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/htags

MAN1=	htags.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${GLOBAL_DIR}/htags/htags.pl ${DESTDIR}${BINDIR}/htags

.include <bsd.prog.mk>
