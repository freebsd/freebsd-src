#
# $FreeBSD$
#

.include <src.opts.mk>

CXGBE=	${.CURDIR}/../../../dev/cxgbe
.PATH: ${CXGBE}/tom

KMOD=	t4_tom
SRCS=	bus_if.h
SRCS+=	device_if.h
SRCS+=	opt_inet.h
SRCS+=	opt_inet6.h
SRCS+=	pci_if.h
SRCS+=	t4_connect.c
SRCS+=	t4_cpl_io.c
SRCS+=	t4_ddp.c
SRCS+=	t4_listen.c
SRCS+=	t4_tom.c
SRCS+=	t4_tom_l2t.c

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
