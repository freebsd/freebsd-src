# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.34.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
