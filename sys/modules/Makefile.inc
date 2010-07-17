# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

