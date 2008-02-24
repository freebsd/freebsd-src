# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.5 2005/12/21 15:49:50 ru Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm amdsmb ichsmb intpm nfsmb viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
