# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	de4ctl \
	flashit \
	mtlctl

.if ${MK_CHERI} != "no"
SUBDIR+=	compositorctl
.endif

.include <bsd.subdir.mk>
