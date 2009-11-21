# $FreeBSD: src/sys/modules/ata/Makefile,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

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
