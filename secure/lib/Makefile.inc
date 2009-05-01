# $FreeBSD: src/secure/lib/Makefile.inc,v 1.9.32.1 2009/04/15 03:14:26 kensmith Exp $

.include "../Makefile.inc"
.if exists(${.CURDIR}/../../../lib/Makefile.inc)
.include "${.CURDIR}/../../../lib/Makefile.inc"
.endif
