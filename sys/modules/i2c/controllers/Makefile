# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.3.4.1 2005/08/19 18:54:24 brian Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm ichsmb intpm viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
