# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.3 2003/06/15 04:24:28 nyan Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm intpm viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
