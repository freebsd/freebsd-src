# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.28.1 2008/10/02 02:57:24 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
