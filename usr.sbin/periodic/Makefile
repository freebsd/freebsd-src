# Makefile for periodic(8)
#
# $FreeBSD$

MAN=	periodic.8

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/periodic.sh ${DESTDIR}${BINDIR}/periodic

.include <bsd.prog.mk>
