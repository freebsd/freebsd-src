#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/idmapd/Makefile,v 1.2 2004/02/23 20:13:52 johan Exp $

PROG=	idmapd
SRCS=	idmapd.c
MAN=	idmapd.8

CFLAGS+= -DNFS -I/usr/src/sys
WARNS?=	2

.PATH:

.include <bsd.prog.mk>
