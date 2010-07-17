# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.34.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
