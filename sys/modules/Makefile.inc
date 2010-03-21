# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.38.1 2010/02/10 00:26:20 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

