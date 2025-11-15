.PATH: ${SRCTOP}/sys/dev/nvmf/host

KMOD=	nvmf

SRCS=	nvmf.c \
	nvmf_aer.c \
	nvmf_cmd.c \
	nvmf_ctldev.c \
	nvmf_ns.c \
	nvmf_qpair.c \
	nvmf_sim.c

SRCS+=	bus_if.h device_if.h
SRCS+=	opt_cam.h

.include <bsd.kmod.mk>
