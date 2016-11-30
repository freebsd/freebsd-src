# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/hyperv/vmbus \
	${.CURDIR}/../../../dev/hyperv/vmbus/${MACHINE_CPUARCH}

KMOD=	hv_vmbus
SRCS=	hyperv.c \
	hyperv_busdma.c \
	hyperv_machdep.c \
	vmbus.c \
	vmbus_br.c \
	vmbus_chan.c \
	vmbus_et.c \
	vmbus_if.c \
	vmbus_xact.c
SRCS+=	acpi_if.h bus_if.h device_if.h opt_acpi.h pci_if.h pcib_if.h vmbus_if.h

# XXX: for assym.s
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_apic.h opt_hwpmc_hooks.h opt_compat.h

SRCS+=	assym.s \
	vmbus_vector.S

vmbus_vector.o:
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
            ${.IMPSRC} -o ${.TARGET}

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus

.include <bsd.kmod.mk>
