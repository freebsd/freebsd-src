#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/idmapd/Makefile,v 1.1 2003/11/14 21:04:33 alfred Exp $

PROG=	idmapd
SRCS=	idmapd.c
MAN=	idmapd.8

CFLAGS+= -DNFS -I/usr/src/sys
WARNS=	2

.PATH:

.include <bsd.prog.mk>
