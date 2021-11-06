# $FreeBSD$

.if defined(MODULES_EXCLUDE) && defined(KMOD) && ${MODULES_EXCLUDE:M${KMOD}}
all:
install:
cleandir:
.else
.include <bsd.sysdir.mk>
.include "${SYSDIR}/conf/kmod.mk"
.endif
