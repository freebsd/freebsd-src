# $FreeBSD: src/sys/modules/ata/Makefile,v 1.3.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

SUBDIR = atacore
SUBDIR += atacard
.if ${MACHINE} == "pc98"
SUBDIR += atacbus
.else
SUBDIR += ataisa
.endif
SUBDIR += atapci 
SUBDIR += atadisk atapicd atapifd atapist ataraid atapicam #atacam

.include <bsd.subdir.mk>
