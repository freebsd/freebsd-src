
PROG=	cpucontrol
MAN=	cpucontrol.8
SRCS=	cpucontrol.c intel.c amd.c amd10h.c via.c ucode_subr.c

.PATH:  ${SRCTOP}/sys/x86/x86

NO_WCAST_ALIGN=

CFLAGS+= -I${.CURDIR}

.include <bsd.prog.mk>
