#
# $FreeBSD$
#

.include <bsd.own.mk>

CXGBE = ${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE} ${CXGBE}/common

KMOD = if_cxgbe
SRCS = t4_main.c t4_sge.c t4_l2t.c t4_tracer.c
SRCS+= t4_hw.c
SRCS+= device_if.h bus_if.h pci_if.h
SRCS+= opt_inet.h opt_inet6.h
SRCS+= opt_ofed.h

CFLAGS+= -I${CXGBE}

# Provide the timestamp of a packet in its header mbuf.
#CFLAGS+= -DT4_PKT_TIMESTAMP

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
