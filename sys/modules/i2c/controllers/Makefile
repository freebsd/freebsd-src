# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
