# $FreeBSD: src/sys/modules/ata/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
