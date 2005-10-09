# $FreeBSD$

.PATH: ${.CURDIR}/../../../geom/vinum

KMOD=	geom_vinum
SRCS=	geom_vinum_drive.c geom_vinum.c geom_vinum_plex.c \
	geom_vinum_volume.c geom_vinum_subr.c geom_vinum_raid5.c \
	geom_vinum_share.c geom_vinum_list.c geom_vinum_rm.c \
	geom_vinum_init.c geom_vinum_state.c

.include <bsd.kmod.mk>
