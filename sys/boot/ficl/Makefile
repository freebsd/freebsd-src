# $FreeBSD$
#

FICLDIR?=	${.CURDIR}

.if defined(FICL32)
.PATH: ${FICLDIR}/${MACHINE_CPUARCH:S/amd64/i386/}
.elif ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el"
.PATH: ${FICLDIR}/mips64
.else
.PATH: ${FICLDIR}/${MACHINE_CPUARCH}
.endif
BASE_SRCS=	dict.c ficl.c fileaccess.c float.c loader.c math64.c \
		prefix.c search.c stack.c tools.c vm.c words.c

SRCS=		${BASE_SRCS} sysdep.c softcore.c
CLEANFILES=	softcore.c testmain testmain.o
.if ${MACHINE_CPUARCH} == "i386" || \
    (${MACHINE_CPUARCH} == "amd64" && defined(FICL32))
CFLAGS+=	-march=i386
.endif
.if HAVE_PNP
CFLAGS+=	-DHAVE_PNP
.endif
.include <bsd.stand.mk>
.ifmake testmain
CFLAGS+=	-DTESTMAIN -D_TESTMAIN
SRCS+=		testmain.c
PROG=		testmain
.include <bsd.prog.mk>
.else
LIB=		ficl
INTERNALLIB=
.include <bsd.lib.mk>
.endif

# Standard softwords
.PATH: ${FICLDIR}/softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

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

.if ${MACHINE_CPUARCH} == "amd64" && defined(FICL32)
FICL_CPUARCH=	i386
.elif ${MACHINE_ARCH} == "mips64" || ${MACHINE_ARCH} == "mips64el"
FICL_CPUARCH=	mips64
.else
FICL_CPUARCH=	${MACHINE_CPUARCH}
.endif

CFLAGS+=	-I${FICLDIR} -I${FICLDIR}/${FICL_CPUARCH} \
		-I${FICLDIR}/../common

softcore.c: ${SOFTWORDS} softcore.awk
	(cd ${FICLDIR}/softwords; cat ${SOFTWORDS} \
	    | awk -f softcore.awk -v datestamp="`LC_ALL=C date`") > ${.TARGET}

.if ${MACHINE_CPUARCH} == "amd64" && defined(FICL32)
.if !exists(machine)
${SRCS:M*.c:R:S/$/.o/g}: machine

beforedepend ${OBJS}: machine
.endif

machine: .NOMETA
	ln -sf ${.CURDIR}/../../i386/include machine

CLEANFILES+=	machine
.endif
