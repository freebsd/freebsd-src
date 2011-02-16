# $FreeBSD: src/sys/modules/ata/Makefile,v 1.2.24.1 2010/12/21 17:10:29 kensmith Exp $

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
