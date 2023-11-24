
.PATH:	${SRCTOP}/sys/dev/mgb

KMOD	= if_mgb
SRCS	= if_mgb.c
SRCS	+= device_if.h bus_if.h pci_if.h
SRCS	+= ifdi_if.h opt_platform.h
SRCS	+= miibus_if.h

#CFLAGS+= -DIFLIB

.include <bsd.kmod.mk>

