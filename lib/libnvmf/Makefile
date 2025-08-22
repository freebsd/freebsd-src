.PATH:  ${SRCTOP}/sys/dev/nvmf/controller
.PATH:  ${SRCTOP}/sys/libkern

LIB=		nvmf
INTERNALLIB=

INCS=		libnvmf.h

SRCS=		gsb_crc32.c \
		nvmf_controller.c \
		nvmf_host.c \
		nvmf_tcp.c \
		nvmf_transport.c \
		nvmft_subr.c

LIBADD=		nv

CFLAGS+=	-I${SRCTOP}/sys/dev/nvmf/controller
CFLAGS+=	-I${SRCTOP}/sys/dev/nvmf

.include <bsd.lib.mk>

CWARNFLAGS.gsb_crc32.c=	-Wno-cast-align
