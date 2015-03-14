# $FreeBSD$

KMOD=	vmm

SRCS=	opt_acpi.h opt_ddb.h device_if.h bus_if.h pci_if.h
SRCS+=	vmx_assym.h svm_assym.h
DPSRCS=	vmx_genassym.c svm_genassym.c

CFLAGS+= -DVMM_KEEP_STATS -DSMP
CFLAGS+= -I${.CURDIR}/../../amd64/vmm
CFLAGS+= -I${.CURDIR}/../../amd64/vmm/io
CFLAGS+= -I${.CURDIR}/../../amd64/vmm/intel
CFLAGS+= -I${.CURDIR}/../../amd64/vmm/amd

# generic vmm support
.PATH: ${.CURDIR}/../../amd64/vmm
SRCS+=	vmm.c		\
	vmm_dev.c	\
	vmm_host.c	\
	vmm_instruction_emul.c	\
	vmm_ioport.c	\
	vmm_lapic.c	\
	vmm_mem.c	\
	vmm_stat.c	\
	vmm_util.c	\
	x86.c

.PATH: ${.CURDIR}/../../amd64/vmm/io
SRCS+=	iommu.c		\
	ppt.c           \
	vatpic.c	\
	vatpit.c	\
	vhpet.c		\
	vioapic.c	\
	vlapic.c	\
	vpmtmr.c	\
	vrtc.c

# intel-specific files
.PATH: ${.CURDIR}/../../amd64/vmm/intel
SRCS+=	ept.c		\
	vmcs.c		\
	vmx_msr.c	\
	vmx_support.S	\
	vmx.c		\
	vtd.c

# amd-specific files
.PATH: ${.CURDIR}/../../amd64/vmm/amd
SRCS+=	vmcb.c		\
	svm.c		\
	svm_support.S	\
	npt.c		\
	amdv.c		\
	svm_msr.c

CLEANFILES=	vmx_assym.h vmx_genassym.o svm_assym.h svm_genassym.o

vmx_assym.h:    vmx_genassym.o
	sh ${SYSDIR}/kern/genassym.sh vmx_genassym.o > ${.TARGET}

svm_assym.h:    svm_genassym.o
	sh ${SYSDIR}/kern/genassym.sh svm_genassym.o > ${.TARGET}

vmx_support.o:
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

svm_support.o:
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
	    ${.IMPSRC} -o ${.TARGET}

vmx_genassym.o:
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

svm_genassym.o:
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

.include <bsd.kmod.mk>
