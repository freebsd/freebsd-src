# $FreeBSD: src/sys/modules/joy/Makefile,v 1.15.2.3 2002/08/07 16:31:56 ru Exp $

.PATH:	${.CURDIR}/../../isa
KMOD	= joy
SRCS	= bus_if.h device_if.h isa_if.h joy.c
MAN	= joy.8

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/joy.sh ${DESTDIR}/usr/bin/joy

.include <bsd.kmod.mk>
