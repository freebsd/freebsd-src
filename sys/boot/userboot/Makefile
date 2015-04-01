# $FreeBSD$

.include <bsd.own.mk>
.include <src.opts.mk>

SUBDIR=		libstand

.if ${MK_FORTH} != "no"
SUBDIR+=	ficl
.endif

.if ${MK_LUA} != "no"
SUBDIR+=	liblua
.endif

SUBDIR+=	test zfs userboot

.include <bsd.subdir.mk>

