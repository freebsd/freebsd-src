# $FreeBSD: src/sys/modules/Makefile.inc,v 1.2.2.2 2002/12/01 09:01:07 nyan Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

