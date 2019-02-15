# $FreeBSD$

.PATH: ${SRCTOP}/sys/netpfil/ipfw/nat64

KMOD=	ipfw_nat64
SRCS=	ip_fw_nat64.c nat64_translate.c
SRCS+=	nat64lsn.c nat64lsn_control.c
SRCS+=	nat64stl.c nat64stl_control.c

.include <bsd.kmod.mk>
