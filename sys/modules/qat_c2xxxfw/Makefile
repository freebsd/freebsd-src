.PATH: ${SRCTOP}/sys/contrib/dev/qat_c2xxx

KMOD=	qat_c2xxxfw
IMG1=	mof_firmware_c2xxx
IMG2=	mmp_firmware_c2xxx

FIRMWS=	${IMG1}.bin:${KMOD}:111 ${IMG2}.bin:${IMG2}:111

.include <bsd.kmod.mk>
