# $FreeBSD$

# Common flags to build FICL related files

.include "defs.mk"

.if ${MACHINE_CPUARCH} == "amd64" && ${DO32:U0} == 1
FICL_CPUARCH=	i386
.elif ${MACHINE_ARCH:Mmips64*} != ""
FICL_CPUARCH=	mips64
.else
FICL_CPUARCH=	${MACHINE_CPUARCH}
.endif

.PATH: ${FICLSRC} ${FICLSRC}/${FICL_CPUARCH}

.if ${MACHINE_CPUARCH} == "amd64"
.if ${DO32:U0} == 1
CFLAGS+=	-m32 -I.
.else
CFLAGS+=	-fPIC
.endif
.endif

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc -I.
.endif

CFLAGS+=	-I${FICLSRC} -I${FICLSRC}/${FICL_CPUARCH} -I${LDRSRC}
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-DBF_DICTSIZE=15000

.if ${MACHINE_CPUARCH} == "amd64" && ${DO32:U0} == 1
.if !exists(machine)
${SRCS:M*.c:R:S/$/.o/g}: machine

beforedepend ${OBJS}: machine
.endif

machine: .NOMETA
	ln -sf ${SYSDIR}/i386/include machine

CLEANFILES+=	machine
.endif
