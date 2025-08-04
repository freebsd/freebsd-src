
.PATH: ${SRCTOP}/sys/amd64/pt

KMOD=	pt
SRCS=	pt.c pt.h device_if.h bus_if.h
SRCS+=	opt_hwpmc_hooks.h opt_kstack_pages.h

.include <bsd.kmod.mk>
