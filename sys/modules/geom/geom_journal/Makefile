# $FreeBSD: src/sys/modules/geom/geom_journal/Makefile,v 1.1 2006/10/31 22:21:47 pjd Exp $

.PATH: ${.CURDIR}/../../../geom/journal

KMOD=	geom_journal
SRCS=	g_journal.c
SRCS+=	g_journal_ufs.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
