# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.4.2.1 2006/08/17 11:01:25 ru Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
