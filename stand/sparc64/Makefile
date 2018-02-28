# $FreeBSD$

NO_OBJ=t

.include <bsd.init.mk>

SUBDIR=	boot1 loader
.if ${MK_ZFS} != "no"
SUBDIR+=zfsboot zfsloader
.endif

.include <bsd.subdir.mk>
