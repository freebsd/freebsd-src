# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/dev/qat

KMOD=	qat_dh895xccfw
IMG1=	qat_895xcc
IMG2=	qat_895xcc_mmp

FIRMWS=	${IMG1}.bin:${IMG1}:111 ${IMG2}.bin:${IMG2}:111

.include <bsd.kmod.mk>
