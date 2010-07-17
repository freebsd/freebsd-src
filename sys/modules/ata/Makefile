# $FreeBSD: src/sys/modules/ata/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
