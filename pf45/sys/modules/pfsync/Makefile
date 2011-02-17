# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../contrib/pf/net

KMOD=	pfsync
SRCS = 	if_pfsync.c \
	opt_pf.h opt_inet.h opt_inet6.h opt_bpf.h

CFLAGS+=  -I${.CURDIR}/../../contrib/pf
SRCS+=  bus_if.h device_if.h

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_bpf.h:
	echo "#define DEV_BPF 1" > ${.TARGET}

.if defined(VIMAGE)
opt_global.h:
	echo "#define VIMAGE 1" > ${.TARGET}
CFLAGS+=	-I${.CURDIR}/opt_global.h
.endif
.endif

.include <bsd.kmod.mk>
