# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

