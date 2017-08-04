# $FreeBSD$

# Common flags to build FICL related files

FICLDIR?=	${SRCTOP}/sys/boot/ficl

.if ${MACHINE_CPUARCH} == "amd64" && defined(FICL32)
FICL_CPUARCH=	i386
.elif ${MACHINE_ARCH:Mmips64*} != ""
FICL_CPUARCH=	mips64
.else
FICL_CPUARCH=	${MACHINE_CPUARCH}
.endif

.PATH: ${FICLDIR} ${FICLDIR}/${FICL_CPUARCH}

.if ${MACHINE_CPUARCH} == "amd64"
.if defined(FICL32)
CFLAGS+=	-m32 -I.
.else
CFLAGS+=	-fPIC
.endif
.endif

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc -I.
.endif

CFLAGS+=	-I${FICLDIR} -I${FICLDIR}/${FICL_CPUARCH} \
		-I${FICLDIR}/../common

.if ${MACHINE_CPUARCH} == "amd64" && defined(FICL32)
.if !exists(machine)
${SRCS:M*.c:R:S/$/.o/g}: machine

beforedepend ${OBJS}: machine
.endif

machine: .NOMETA
	ln -sf ${.CURDIR}/../../i386/include machine

CLEANFILES+=	machine
.endif
