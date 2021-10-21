# $Cronyx: sys.modules.cp.Makefile,v 1.1.2.3 2004/04/28 16:17:57 rik Exp $
# $FreeBSD$

.PATH:  ${SRCTOP}/sys/dev/cp
KMOD	= if_cp
SRCS	= if_cp.c cpddk.c
SRCS   += device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>

CWARNFLAGS.cpddk.c+=	${NO_WMISLEADING_INDENTATION}
