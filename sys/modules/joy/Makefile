# $FreeBSD$

.PATH:	${.CURDIR}/../../isa
KMOD	= joy
SRCS	= bus_if.h device_if.h isa_if.h joy.c
.if exists(${DESTDIR}/usr/share/man/man8)
MAN8	= joy.8
.endif

.if exists(${DESTDIR}/usr/bin)
afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/joy.sh ${DESTDIR}/usr/bin/joy
.endif

.include <bsd.kmod.mk>
