# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/dev/oltr ${.CURDIR}/../../net
KMOD	= if_oltr
SRCS	= if_oltr.c trlldbm.c trlldhm.c trlldmac.c if_iso88025subr.c \
	  opt_inet.h opt_inet6.h opt_ipx.h device_if.h bus_if.h pci_if.h
OBJS+=	trlld.o

TR_INET?=	1	# 0/1 requires INET to be configured in the kernel
TR_INET6?=	0	# 0/1 requires INET6 to be configured in the kernel
TR_IPX?=	0	# 0/1 requires IPX to be configured in the kernel

trlld.o:
	uudecode < ${.CURDIR}/../../contrib/dev/oltr/i386-elf.trlld.o.uu

opt_inet.h:
	touch opt_inet.h
.if ${TR_INET} > 0
	echo "#define INET 1" > opt_inet.h
.endif

opt_inet6.h:
	touch opt_inet6.h
.if ${TR_INET6} > 0
	echo "#define INET6 1" > opt_inet6.h
.endif

opt_ipx.h:
	touch opt_ipx.h
.if ${TR_IPX} > 0
	echo "#define IPX 1" > opt_ipx.h
.endif

.include <bsd.kmod.mk>
