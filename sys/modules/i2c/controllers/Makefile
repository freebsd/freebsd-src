# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.14.1 2010/12/21 17:10:29 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
