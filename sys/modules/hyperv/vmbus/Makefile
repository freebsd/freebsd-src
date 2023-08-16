
.PATH:	${SRCTOP}/sys/dev/hyperv/vmbus \
	${SRCTOP}/sys/dev/hyperv/vmbus/${MACHINE_CPUARCH} \
	${SRCTOP}/sys/dev/hyperv/vmbus/x86

KMOD=	hv_vmbus
SRCS=	hyperv.c \
	hyperv_busdma.c \
	hyperv_machdep.c \
	vmbus.c \
	vmbus_br.c \
	vmbus_chan.c \
	vmbus_if.c \
	vmbus_res.c \
	vmbus_xact.c

.if ${MACHINE_CPUARCH} != "i386" && ${MACHINE_CPUARCH} != "aarch64"
SRCS+=	vmbus_vector.S
.endif
.if ${MACHINE_CPUARCH} != "aarch64"
SRCS+= vmbus_et.c hyperv_x86.c vmbus_x86.c
.else
SRC+= hyperv_aarch64.c vmbus_aarch64.c
.endif
SRCS+=	acpi_if.h bus_if.h device_if.h opt_acpi.h pci_if.h pcib_if.h vmbus_if.h

# XXX: for assym.inc
SRCS+=  opt_kstack_pages.h opt_nfs.h opt_hwpmc_hooks.h
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	opt_apic.h
.endif

DPSRCS=	assym.inc

vmbus_vector.o:
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
            ${.IMPSRC} -o ${.TARGET}

CFLAGS+= -I${SRCTOP}/sys/dev/hyperv/include \
	 -I${SRCTOP}/sys/dev/hyperv/vmbus

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
