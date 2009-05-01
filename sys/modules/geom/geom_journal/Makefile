# $FreeBSD: src/sys/modules/geom/geom_journal/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../geom/journal

KMOD=	geom_journal
SRCS=	g_journal.c
SRCS+=	g_journal_ufs.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
