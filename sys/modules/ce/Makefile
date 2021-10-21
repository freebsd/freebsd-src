# $Cronyx: sys.modules.ce.Makefile,v 1.2 2005/04/23 20:10:22 rik Exp $
# $FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/ce
KMOD	= if_ce
SRCS	= if_ce.c ceddk.c tau32-ddk.c
SRCS   += device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

CWARNFLAGS.tau32-ddk.c+=	${NO_WCONSTANT_CONVERSION}
CWARNFLAGS.tau32-ddk.c+=	${NO_WMISLEADING_INDENTATION}
