
.PATH: ${SRCTOP}/sys/dev/random

KMOD=	rdrand_rng
SRCS=	ivy.c
SRCS+=	bus_if.h device_if.h

CFLAGS+= -I${SRCTOP}/sys

# ld.bfd doesn't support ifuncs invoked non-PIC
.if ${MACHINE_CPUARCH} == "i386"
CFLAGS.gcc= -fPIC
.endif

.include <bsd.kmod.mk>
