# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

