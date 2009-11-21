# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

