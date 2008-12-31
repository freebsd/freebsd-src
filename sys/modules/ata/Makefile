# $FreeBSD: src/sys/modules/ata/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

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
