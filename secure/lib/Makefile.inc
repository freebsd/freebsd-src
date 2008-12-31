# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.30.1 2008/11/25 02:59:29 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
