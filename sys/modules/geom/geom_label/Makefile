# $FreeBSD: src/sys/modules/geom/geom_label/Makefile,v 1.1 2004/07/02 19:40:34 pjd Exp $

.PATH: ${.CURDIR}/../../../geom/label

KMOD=	geom_label
SRCS=	g_label.c
SRCS+=	g_label_iso9660.c
SRCS+=	g_label_msdosfs.c
SRCS+=	g_label_ufs.c

.include <bsd.kmod.mk>
