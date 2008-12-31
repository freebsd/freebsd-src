# $FreeBSD: src/sys/modules/geom/geom_journal/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../geom/journal

KMOD=	geom_journal
SRCS=	g_journal.c
SRCS+=	g_journal_ufs.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
