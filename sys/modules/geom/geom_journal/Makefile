# $FreeBSD: src/sys/modules/geom/geom_journal/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../geom/journal

KMOD=	geom_journal
SRCS=	g_journal.c
SRCS+=	g_journal_ufs.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
