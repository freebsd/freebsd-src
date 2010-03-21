# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.12.1 2010/02/10 00:26:20 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
