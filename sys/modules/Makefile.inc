# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4.32.1 2008/11/25 02:59:29 kensmith Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

