# $FreeBSD: src/sys/modules/geom/geom_label/Makefile,v 1.1.8.1 2005/08/16 08:20:48 pjd Exp $

.PATH: ${.CURDIR}/../../../geom/label

KMOD=	geom_label
SRCS=	g_label.c
SRCS+=	g_label_ext2fs.c
SRCS+=	g_label_iso9660.c
SRCS+=	g_label_msdosfs.c
SRCS+=	g_label_reiserfs.c
SRCS+=	g_label_ufs.c

.include <bsd.kmod.mk>
