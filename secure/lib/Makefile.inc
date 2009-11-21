# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.34.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
