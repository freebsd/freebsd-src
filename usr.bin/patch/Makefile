# $FreeBSD$
#	$OpenBSD: Makefile,v 1.4 2005/05/16 15:22:46 espie Exp $
#	$DragonFly: src/usr.bin/patch/Makefile,v 1.8 2008/08/10 23:50:12 joerg Exp $

.include <bsd.own.mk>

.if ${MK_BSD_PATCH} == "yes"
PROG=	patch
.else
PROG=	bsdpatch
CLEANFILES+= bsdpatch.1

bsdpatch.1: patch.1
	cp ${.ALLSRC} ${.TARGET}
.endif

SRCS=	backupfile.c inp.c mkpath.c patch.c pch.c util.c

.include <bsd.prog.mk>
