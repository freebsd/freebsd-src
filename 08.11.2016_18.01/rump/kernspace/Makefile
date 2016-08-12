#	$NetBSD: Makefile,v 1.5 2011/01/14 13:08:00 pooka Exp $
#

.include <bsd.own.mk>

LIB=	kernspace
SRCS=	thread.c busypage.c tsleep.c alloc.c lockme.c sendsig.c

RUMPTOP=${NETBSDSRCDIR}/sys/rump

LIBISPRIVATE=

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
