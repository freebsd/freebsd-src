# $FreeBSD: src/sys/modules/geom/geom_vinum/Makefile,v 1.4 2005/11/24 15:11:41 le Exp $

.PATH: ${.CURDIR}/../../../geom/vinum

KMOD=	geom_vinum
SRCS=	geom_vinum.c geom_vinum_drive.c geom_vinum_plex.c \
	geom_vinum_volume.c geom_vinum_subr.c geom_vinum_raid5.c \
	geom_vinum_share.c geom_vinum_list.c geom_vinum_rm.c \
	geom_vinum_init.c geom_vinum_state.c geom_vinum_rename.c \
	geom_vinum_move.c

.include <bsd.kmod.mk>
