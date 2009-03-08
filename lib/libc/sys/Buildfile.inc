# $FreeBSD$

# sys sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/sys ${.CURDIR}/sys

# Include the generated makefile containing the *complete* list
# of syscall names in MIASM.
.include "${BSDSRCTOP}/sys/sys/syscall.mk"

# Include machine dependent definitions.
#
# MDASM names override the default syscall names in MIASM.
# NOASM will prevent the default syscall code from being generated.
#
.if exists(${.CURDIR}/${MACHINE_ARCH}/sys/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/sys/Makefile.inc"
.endif

# Sources common to both syscall interfaces:
SRCS += \
	__error.c \
	stack_protector.c

.if !defined(WITHOUT_SYSCALL_COMPAT)
SRCS += \
	fcntl.c \
	ftruncate.c \
	lseek.c \
	mmap.c \
	pread.c \
	pwrite.c \
	truncate.c
PSEUDO += _fcntl.o
.endif

# Add machine dependent asm sources:
SRCS+=${MDASM}

# Look though the complete list of syscalls (MIASM) for names that are
# not defined with machine dependent implementations (MDASM) and are
# not declared for no generation of default code (NOASM).  Add each
# syscall that satisfies these conditions to the ASM list.
.for _asm in ${MIASM}
.if (${MDASM:R:M${_asm:R}} == "")
.if (${NOASM:R:M${_asm:R}} == "")
.if (${SRCS:R:M${_asm:R}} == "")
ASM+=$(_asm)
.endif
.endif
.endif
.endfor

OBJS +=	${ASM} ${PSEUDO}

SASM =	${ASM:S/.o/.S/}

SPSEUDO = ${PSEUDO:S/.o/.S/}

SRCS += \
	${SASM} \
	${SPSEUDO}

SYM_MAPS += ${.CURDIR}/sys/Symbol.map

# Generated files
${SASM}:
	printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' > ${.TARGET}

${SPSEUDO}:
	printf '#include "SYS.h"\nPSEUDO(${.PREFIX:S/_//})\n' \
	    > ${.TARGET}
