#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.10 1998/03/13 19:39:14 jb Exp $

SUBDIR= cat chio chmod cp csh date dd domainname echo ed expr hostname \
	kill ln ls mkdir mv pax pwd rcp rm rmail rmdir sh sleep \
	stty sync test

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=df ps
.endif

.include <bsd.subdir.mk>
