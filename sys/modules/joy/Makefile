# $FreeBSD: src/sys/modules/joy/Makefile,v 1.15 1999/12/12 20:40:56 peter Exp $

.PATH:	${.CURDIR}/../../isa
KMOD	= joy
SRCS	= bus_if.h device_if.h isa_if.h joy.c
MAN8	= joy.8

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/joy.sh ${DESTDIR}/usr/bin/joy

.include <bsd.kmod.mk>
