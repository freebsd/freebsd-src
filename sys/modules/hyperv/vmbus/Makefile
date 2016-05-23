# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/hyperv/vmbus \
	${.CURDIR}/../../../dev/hyperv/vmbus/${MACHINE_CPUARCH}

KMOD=	hv_vmbus
SRCS=	hv_channel.c \
	hv_channel_mgmt.c \
	hv_connection.c \
	hv_et.c \
	hv_hv.c \
	hv_ring_buffer.c \
	hv_vmbus_drv_freebsd.c \
	hv_vmbus_priv.h \
	hyperv_busdma.c
SRCS+=	acpi_if.h bus_if.h device_if.h opt_acpi.h

# XXX: for assym.s
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_apic.h opt_hwpmc_hooks.h opt_compat.h

SRCS+=	assym.s \
	hv_vector.S

hv_vector.o:
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
            ${.IMPSRC} -o ${.TARGET}

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus

.include <bsd.kmod.mk>
