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
	vmbus_et.c
SRCS+=	acpi_if.h bus_if.h device_if.h opt_acpi.h vmbus_if.h

# XXX: for assym.s
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_hwpmc_hooks.h opt_compat.h
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	opt_apic.h
.endif

SRCS+=	assym.s \
	vmbus_vector.S

vmbus_vector.o:
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
            ${.IMPSRC} -o ${.TARGET}

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus

.include <bsd.kmod.mk>
