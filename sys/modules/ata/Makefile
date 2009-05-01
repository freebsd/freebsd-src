# $FreeBSD: src/sys/modules/ata/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

SUBDIR = ata
SUBDIR += atacard
.if ${MACHINE} == "pc98"
SUBDIR += atacbus
.else
SUBDIR += ataisa
.endif
SUBDIR += atapci 
SUBDIR += atadisk atapicd atapifd atapist ataraid atapicam #atacam

.include <bsd.subdir.mk>
