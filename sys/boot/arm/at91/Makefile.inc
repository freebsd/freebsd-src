# $FreeBSD$

.if !target(__at91_boot_Makefile.inc__)
.PATH:	${.CURDIR}/../../../../libkern ${.CURDIR}/../../../../libkern/arm

__at91_boot_Makefile.inc__:

CFLAGS=-O2 -mcpu=arm9 -ffreestanding \
	-I${.CURDIR}/../libat91 \
	-I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../arm \
	-D_KERNEL \
	-Wall -Waggregate-return  \
	-Wnested-externs \
	-Wpointer-arith -Wshadow -Wwrite-strings \
	-Werror
# -Wmissing-prototypes
# -Wmissing-declarations 
# -Wstrict-prototypes 

#CFLAGS+=-DBOOT_TSC
CFLAGS+=-DBOOT_KB9202

LIBAT91=${.OBJDIR}/../libat91/libat91.a

LD ?= ld
OBJCOPY ?= objcopy

.if defined(P)
${P}:	${P}.out
	${OBJCOPY} -S -O binary ${P}.out ${.TARGET}
	@set -- `ls -l ${.TARGET}`; x=$$((12288-$$5)); \
	    echo "$$x bytes available"; test $$x -ge 0

${P}.out: ${OBJS}
	${LD} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LIBAT91}

CLEANFILES+= ${P} ${P}.out
.endif

.if defined(WITH_TAG_LIST)
MK_TAG_LIST:=yes
.else
MK_TAG_LIST:=no
.endif

.if defined(WITHOUT_FPGA)
MK_FPGA:=no
.else
MK_FPGA:=yes
.endif

.endif
