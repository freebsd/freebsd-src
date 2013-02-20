# $FreeBSD$

.include <bsd.own.mk>

.if ${MK_BSD_PATCH} == "yes"
PROG=	gnupatch
CLEANFILES+= gnupatch.1

gnupatch.1: patch.1
	cp ${.ALLSRC} ${.TARGET}
.else
PROG=	patch
.endif

SRCS=   backupfile.c inp.c patch.c pch.c util.c version.c
CFLAGS+=-DHAVE_CONFIG_H

.include <bsd.prog.mk>
