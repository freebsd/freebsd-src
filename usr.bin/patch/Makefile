#	$OpenBSD: Makefile,v 1.4 2005/05/16 15:22:46 espie Exp $
# $FreeBSD$

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
