# Makefile
# $Id: Makefile,v 1.4.6.1 1996/06/23 22:03:53 wosch Exp $

MAN8=	manctl.8

beforeinstall: 
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/manctl.sh ${DESTDIR}${BINDIR}/manctl

.include <bsd.prog.mk>
