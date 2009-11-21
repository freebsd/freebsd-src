# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
