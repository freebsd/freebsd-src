
.include <kmod.opts.mk>

KMOD=	vmm

SRCS=	opt_acpi.h opt_bhyve_snapshot.h opt_ddb.h \
	acpi_if.h bus_if.h device_if.h pci_if.h pcib_if.h vnode_if.h

CFLAGS+= -DVMM_KEEP_STATS
CFLAGS+= -I${SRCTOP}/sys/${MACHINE}/vmm
CFLAGS+= -I${SRCTOP}/sys/${MACHINE}/vmm/io

# generic vmm support
.PATH: ${SRCTOP}/sys/${MACHINE}/vmm
SRCS+=	vmm.c		\
	vmm_dev.c	\
	vmm_instruction_emul.c	\
	vmm_stat.c

.if ${MACHINE_CPUARCH} == "aarch64"
DPSRCS+=	assym.inc

# TODO: Add the new EL2 code
SRCS+=	vmm_arm64.c \
	vmm_reset.c \
	vmm_call.S \
	vmm_mmu.c \
	vmm_hyp_el2.S

.PATH: ${SRCTOP}/sys/${MACHINE}/vmm/io
SRCS+=	vgic.c \
	vgic_if.h \
	vgic_if.c \
	vgic_v3.c \
	vtimer.c

SRCS+=	vmm_hyp_exception.S vmm_hyp.c

CLEANFILES+=	vmm_hyp_blob.elf.full
CLEANFILES+=	vmm_hyp_blob.elf vmm_hyp_blob.bin

vmm_hyp_exception.o: vmm_hyp_exception.S
	${CC} -c -x assembler-with-cpp -DLOCORE \
		${CFLAGS:N-fsanitize*:N-fno-sanitize*:N-mbranch-protection*} \
	    ${.IMPSRC} -o ${.TARGET} -fpie

vmm_hyp.o: vmm_hyp.c
	${CC} -c ${CFLAGS:N-fsanitize*:N-fno-sanitize*:N-mbranch-protection*} \
	    ${.IMPSRC} -o ${.TARGET} -fpie

vmm_hyp_blob.elf.full:	vmm_hyp_exception.o vmm_hyp.o
	${LD} -m ${LD_EMULATION} -Bdynamic -T ${SYSDIR}/conf/ldscript.arm64 \
	${_LDFLAGS} --no-warn-mismatch --warn-common --export-dynamic \
	--dynamic-linker /red/herring -X -o ${.TARGET} ${.ALLSRC} \
	--defsym=text_start='0x0'

vmm_hyp_blob.elf:	vmm_hyp_blob.elf.full
	${OBJCOPY} --strip-debug ${.ALLSRC} ${.TARGET}

vmm_hyp_blob.bin:	vmm_hyp_blob.elf
	${OBJCOPY} --output-target=binary ${.ALLSRC} ${.TARGET}

vmm_hyp_el2.o:	vmm_hyp_blob.bin

.elif ${MACHINE_CPUARCH} == "amd64"
DPSRCS+=	vmx_assym.h svm_assym.h
DPSRCS+=	vmx_genassym.c svm_genassym.c offset.inc

CFLAGS+= -I${SRCTOP}/sys/amd64/vmm/intel
CFLAGS+= -I${SRCTOP}/sys/amd64/vmm/amd

SRCS+=	vmm_host.c	\
	vmm_ioport.c	\
	vmm_lapic.c	\
	vmm_mem.c	\
	vmm_util.c	\
	x86.c

.PATH: ${SRCTOP}/sys/${MACHINE}/vmm/io
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
.PATH: ${SRCTOP}/sys/amd64/vmm/intel
SRCS+=	ept.c		\
	vmcs.c		\
	vmx_msr.c	\
	vmx_support.S	\
	vmx.c		\
	vtd.c

# amd-specific files
.PATH: ${SRCTOP}/sys/amd64/vmm/amd
SRCS+=	vmcb.c		\
	amdiommu.c	\
	ivhd_if.c	\
	ivhd_if.h	\
	svm.c		\
	svm_support.S	\
	npt.c		\
	ivrs_drv.c	\
	amdvi_hw.c	\
	svm_msr.c

SRCS.BHYVE_SNAPSHOT=	vmm_snapshot.c

CLEANFILES+=	vmx_assym.h vmx_genassym.o svm_assym.h svm_genassym.o

OBJS_DEPEND_GUESS.vmx_support.o+= vmx_assym.h
OBJS_DEPEND_GUESS.svm_support.o+= svm_assym.h
.endif

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

hyp_genassym.o: offset.inc
	${CC} -c ${CFLAGS:N-flto:N-fno-common} -fcommon ${.IMPSRC}

vmx_genassym.o: offset.inc
	${CC} -c ${CFLAGS:N-flto*:N-fno-common:N-fsanitize*:N-fno-sanitize*} \
	    -fcommon ${.IMPSRC}

svm_genassym.o: offset.inc
	${CC} -c ${CFLAGS:N-flto*:N-fno-common:N-fsanitize*:N-fno-sanitize*} \
	    -fcommon ${.IMPSRC}

.include <bsd.kmod.mk>
