#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.9 1997/12/07 02:26:12 peter Exp $

SUBDIR= cat chio chmod cp date dd domainname echo ed expr hostname \
	kill ln ls mkdir mv pax pwd rcp rm rmail rmdir sleep \
	stty sync test

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=csh df ps sh
.endif

.include <bsd.subdir.mk>
