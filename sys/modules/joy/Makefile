# $FreeBSD$

.PATH:	${.CURDIR}/../../isa
KMOD	= joy
SRCS	= bus_if.h device_if.h isa_if.h joy.c
MAN8	= joy.8

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/joy.sh ${DESTDIR}/usr/bin/joy

.include "${.CURDIR}/../../conf/kmod.mk"
