# $FreeBSD: src/sys/modules/geom/geom_label/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../geom/label

KMOD=	geom_label
SRCS=	g_label.c
SRCS+=	g_label_ext2fs.c
SRCS+=	g_label_iso9660.c
SRCS+=	g_label_msdosfs.c
SRCS+=	g_label_ntfs.c
SRCS+=	g_label_reiserfs.c
SRCS+=	g_label_ufs.c

.include <bsd.kmod.mk>
