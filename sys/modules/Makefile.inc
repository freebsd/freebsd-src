# $FreeBSD: src/sys/modules/Makefile.inc,v 1.4 2002/11/06 13:46:58 nyan Exp $

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

