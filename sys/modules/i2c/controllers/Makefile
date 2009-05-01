# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
