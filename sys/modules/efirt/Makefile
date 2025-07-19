.PATH: ${SRCTOP}/sys/${MACHINE}/${MACHINE}
.PATH: ${SRCTOP}/sys/dev/efidev

KMOD=	efirt
SRCS=	efirt.c efirt_machdep.c efidev.c
SRCS+=	efirtc.c
SRCS+=	efirt_support.S
SRCS+=  device_if.h bus_if.h clock_if.h
DPSRCS+= assym.inc

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	opt_acpi.h opt_hwpmc_hooks.h opt_kstack_pages.h
.endif

efirt_support.o:	efirt_support.S assym.inc
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
