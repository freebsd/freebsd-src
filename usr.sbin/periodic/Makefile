# Makefile for periodic(8)
#
# $Id$

MAN8=	periodic.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/periodic.sh ${DESTDIR}${BINDIR}/periodic

.include <bsd.prog.mk>
