#
# $FreeBSD$
#

.include <bsd.own.mk>

CXGBE = ${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE}/tom

KMOD = t4_tom
SRCS = t4_tom.c t4_connect.c t4_listen.c t4_cpl_io.c t4_tom_l2t.c t4_ddp.c
SRCS+= device_if.h bus_if.h pci_if.h
SRCS+= opt_inet.h opt_inet6.h

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
