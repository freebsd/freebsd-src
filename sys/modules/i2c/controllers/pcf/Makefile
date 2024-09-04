.PATH: ${SRCTOP}/sys/dev/pcf

KMOD=	pcf
SRCS=	pcf.c ${pcf_isa}
SRCS+=	bus_if.h device_if.h iicbus_if.h ${isa_if}

.if ${MACHINE_CPUARCH} == "i386"
isa_if=		isa_if.h
pcf_isa=	pcf_isa.c
.endif

.include <bsd.kmod.mk>
