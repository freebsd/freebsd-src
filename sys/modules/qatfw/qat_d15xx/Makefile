# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD=	qat_d15xxfw
IMG1=	qat_d15xx
IMG2=	qat_d15xx_mmp

FIRMWS=	${IMG1}.bin:${KMOD}:111 ${IMG2}.bin:${IMG2}:111

.include <bsd.kmod.mk>
