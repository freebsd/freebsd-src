# $FreeBSD: src/sys/modules/ata/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

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
