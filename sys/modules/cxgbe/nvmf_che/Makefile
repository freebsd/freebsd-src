
CXGBE = ${SRCTOP}/sys/dev/cxgbe
.PATH: ${CXGBE}/nvmf

KMOD=	nvmf_che

SRCS=	nvmf_che.c
SRCS+=	opt_inet.h

CFLAGS+= -I${CXGBE}

.include <bsd.kmod.mk>
