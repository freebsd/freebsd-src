# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

