
CXGBE = ${SRCTOP}/sys/dev/cxgbe
.PATH: ${CXGBE}/nvmf

KMOD=	nvmf_che

SRCS=	\
	nvmf_che.c \
	opt_inet.h \
	bus_if.h \
	device_if.h \
	pci_if.h \

CFLAGS+= -I${CXGBE}

.include <bsd.kmod.mk>
