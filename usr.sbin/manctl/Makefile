# Makefile
# $Id: Makefile,v 1.9 1997/02/22 16:06:27 peter Exp $

MAN8=	manctl.8

beforeinstall: 
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/manctl.sh ${DESTDIR}${BINDIR}/manctl

.include <bsd.prog.mk>
