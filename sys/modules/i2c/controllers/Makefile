# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.4.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
