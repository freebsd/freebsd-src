# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.40.1 2010/12/21 17:10:29 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

