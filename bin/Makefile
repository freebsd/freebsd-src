#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.11 1998/05/09 06:42:52 jb Exp $

SUBDIR= cat chio chmod cp csh date dd domainname echo ed expr hostname \
	kill ln ls mkdir mv pax pwd rcp rm rmdir sh sleep \
	stty sync test

.if !defined(NO_SENDMAIL)
SUBDIR+=rmail
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=df ps
.endif

.include <bsd.subdir.mk>
