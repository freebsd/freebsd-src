# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD=	qat_c3xxxfw
IMG1=	qat_c3xxx
IMG2=	qat_c3xxx_mmp

FIRMWS=	${IMG1}.bin:${KMOD}:111 ${IMG2}.bin:${IMG2}:111

.include <bsd.kmod.mk>
