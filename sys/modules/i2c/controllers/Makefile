# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
