# $NetBSD: Makefile,v 1.2 2003/01/22 02:56:30 lukem Exp $

.include <bsd.own.mk>

PROG=	progress
SRCS=	progress.c progressbar.c

CPPFLAGS+=-I${NETBSDSRCDIR}/usr.bin/ftp -DSTANDALONE_PROGRESS

.PATH:	${NETBSDSRCDIR}/usr.bin/ftp

.include <bsd.prog.mk>
