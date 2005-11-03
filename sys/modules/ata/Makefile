# $FreeBSD: src/sys/modules/ata/Makefile,v 1.2 2005/04/05 02:05:01 scottl Exp $

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
