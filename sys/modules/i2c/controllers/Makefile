# $FreeBSD: src/sys/modules/i2c/controllers/Makefile,v 1.4 2005/06/05 12:00:03 takawata Exp $

.if ${MACHINE} == "pc98"
SUBDIR = lpbb
.else
SUBDIR = alpm amdpm ichsmb intpm viapm lpbb pcf
.endif

.include <bsd.subdir.mk>
