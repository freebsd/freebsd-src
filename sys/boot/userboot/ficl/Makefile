# $FreeBSD$
#
.include <bsd.own.mk>
MK_SSP=		no

.PATH: ${.CURDIR}/../../ficl
.PATH: ${.CURDIR}/../../ficl/${MACHINE_CPUARCH}
BASE_SRCS=	dict.c ficl.c fileaccess.c float.c loader.c math64.c \
		prefix.c search.c stack.c tools.c vm.c words.c

SRCS=		${BASE_SRCS} sysdep.c softcore.c
CLEANFILES=	softcore.c testmain testmain.o
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
.PATH: ${.CURDIR}/../../ficl/softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

#.if ${MACHINE_CPUARCH} == "amd64"
#CFLAGS+=	-m32 -I.
#.endif

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc -I.
.endif

CFLAGS+=	-I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/${MACHINE_CPUARCH}
CFLAGS+=	-I${.CURDIR}/../../common

softcore.c: ${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/../../ficl/softwords; cat ${SOFTWORDS} \
	    | awk -f softcore.awk -v datestamp="`LC_ALL=C date`") > ${.TARGET}

#.if ${MACHINE_CPUARCH} == "amd64"
#${SRCS:M*.c:R:S/$/.o/g}: machine
#
#beforedepend ${OBJS}: machine
#
#machine: .NOMETA
#	ln -sf ${.CURDIR}/../../i386/include machine
#
#CLEANFILES+=	machine
#.endif
