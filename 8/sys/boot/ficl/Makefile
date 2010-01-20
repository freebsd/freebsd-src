# $FreeBSD$
#
.PATH: ${.CURDIR}/${MACHINE_ARCH:S/amd64/i386/}
BASE_SRCS=	dict.c ficl.c fileaccess.c float.c loader.c math64.c \
		prefix.c search.c stack.c tools.c vm.c words.c

SRCS=		${BASE_SRCS} sysdep.c softcore.c
CLEANFILES=	softcore.c testmain testmain.o
CFLAGS+=	-ffreestanding
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-mpreferred-stack-boundary=2
CFLAGS+=	-mno-mmx -mno-3dnow -mno-sse -mno-sse2
.endif
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-mno-sse3
.endif
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "arm"
CFLAGS+=	-msoft-float
.endif
.if ${MACHINE} == "pc98"
CFLAGS+=	-Os -DPC98
.endif
.if HAVE_PNP
CFLAGS+=	-DHAVE_PNP
.endif
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
.PATH: ${.CURDIR}/softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-m32 -march=i386 -I.
.endif

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH:S/amd64/i386/} \
		-I${.CURDIR}/../common

softcore.c: ${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} \
	    | awk -f softcore.awk -v datestamp="`LC_ALL=C date`") > ${.TARGET}

.if ${MACHINE_ARCH} == "amd64"
${SRCS:M*.c:R:S/$/.o/g}: machine

beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../i386/include machine

CLEANFILES+=	machine
.endif
