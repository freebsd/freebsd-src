# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.38.1 2010/12/21 17:10:29 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
