# $FreeBSD: src/share/examples/isdn/i4brunppp/Makefile,v 1.1.2.1 2001/08/12 01:57:09 obrien Exp $

PROG	 =	i4brunppp
SRCS	 =	i4brunppp.c
MAN8	 =	i4brunppp.8

RUNPPPDEST=	/etc/isdn

INSTALL	?=	install

install:
	@echo i4brunppp is not installed automatically. In case you want to run
	@echo user-ppp in dial-in server under i4b, please copy it into /etc/isdn.

installetc:	i4brunppp
	${INSTALL} -c -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} \
		${.CURDIR}/i4brunppp ${RUNPPPDEST}

.include <bsd.prog.mk>
