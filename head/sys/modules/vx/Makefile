# $FreeBSD$

SYSDIR?=${.CURDIR}/../..
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/dev/vx

KMOD=	if_vx
SRCS=	if_vx.c if_vx_pci.c
SRCS.DEV_EISA=	if_vx_eisa.c eisa_if.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
