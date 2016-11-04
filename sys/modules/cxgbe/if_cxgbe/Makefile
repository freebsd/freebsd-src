#
# $FreeBSD$
#

.include <bsd.own.mk>

CXGBE=	${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE} ${CXGBE}/common

KMOD=	if_cxgbe
SRCS=	bus_if.h
SRCS+=	device_if.h
SRCS+=	opt_ddb.h
SRCS+=	opt_inet.h
SRCS+=	opt_inet6.h
SRCS+=	opt_ofed.h
SRCS+=	pci_if.h
SRCS+=	t4_hw.c
SRCS+=	t4_l2t.c
SRCS+=	t4_main.c
SRCS+=	t4_mp_ring.c
SRCS+=	t4_netmap.c
SRCS+=	t4_sge.c
SRCS+=	t4_tracer.c

# Provide the timestamp of a packet in its header mbuf.
#CFLAGS+= -DT4_PKT_TIMESTAMP

CFLAGS+= -I${CXGBE}

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
	@echo "#define TCP_OFFLOAD 1" >> ${.TARGET}
.endif

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
CFLAGS+= ${GCC_MS_EXTENSIONS}
