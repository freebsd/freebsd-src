# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
