# Makefile
# $Id: Makefile,v 1.6 1995/07/25 00:37:23 bde Exp $

MAN8=	manctl.8

beforeinstall: 
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/manctl.sh ${DESTDIR}${BINDIR}/manctl

.include <bsd.prog.mk>
