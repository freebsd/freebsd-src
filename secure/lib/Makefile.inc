# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.36.1 2010/02/10 00:26:20 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
