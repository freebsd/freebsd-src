# Makefile for periodic(8)
#
# $Id: Makefile,v 1.1.1.1 1997/08/12 17:48:49 pst Exp $

MAN8=	periodic.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/periodic.sh ${DESTDIR}${BINDIR}/periodic

.include <bsd.prog.mk>
