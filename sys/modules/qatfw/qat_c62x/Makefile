# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD=	qat_c62xfw
IMG1=	qat_c62x
IMG2=	qat_c62x_mmp

FIRMWS=	${IMG1}.bin:${IMG1}:111 ${IMG2}.bin:${IMG2}:111

.include <bsd.kmod.mk>
