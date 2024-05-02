.include <src.opts.mk>
.PATH:  ${SRCTOP}/sys/libkern

PACKAGE=nvme-tools
PROG=	nvmfd
SRCS=	nvmfd.c controller.c ctl.c devices.c discovery.c gsb_crc32.c io.c
CFLAGS+= -I${SRCTOP}/lib/libnvmf
MAN=	nvmfd.8
LIBADD+= nvmf pthread util nv

.include <bsd.prog.mk>

CFLAGS.ctl.c=	-I${SRCTOP}/sys
CWARNFLAGS.gsb_crc32.c=	-Wno-cast-align
