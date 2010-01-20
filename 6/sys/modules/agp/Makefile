# $FreeBSD$

.PATH: ${.CURDIR}/../../pci

KMOD=	agp
SRCS=	agp.c agp_if.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=	agp_i810.c agp_intel.c agp_via.c agp_sis.c agp_ali.c agp_amd.c \
	agp_nvidia.c agp_ati.c
.endif
.if ${MACHINE} == "i386"
SRCS+=	agp_amd64.c
.endif
.if  ${MACHINE_ARCH} == "alpha"
SRCS+=	agp_amd.c
.endif
.if  ${MACHINE_ARCH} == "amd64"
SRCS+=	agp_amd64.c agp_i810.c
.endif
SRCS+=	device_if.h bus_if.h agp_if.h pci_if.h
SRCS+=	opt_bdg.h opt_bus.h
MFILES=	kern/device_if.m kern/bus_if.m pci/agp_if.m dev/pci/pci_if.m
WERROR=

EXPORT_SYMS=	agp_find_device		\
		agp_state		\
		agp_acquire		\
		agp_release		\
		agp_enable		\
		agp_alloc_memory	\
		agp_free_memory		\
		agp_bind_memory		\
		agp_unbind_memory	\
		agp_memory_info

.include <bsd.kmod.mk>
