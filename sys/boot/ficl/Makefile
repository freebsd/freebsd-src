# $FreeBSD: src/sys/boot/ficl/Makefile,v 1.17.2.5 2001/03/05 13:08:45 ru Exp $
#
.PATH:			${.CURDIR}/${MACHINE_ARCH}
BASE_SRCS=		dict.c ficl.c math64.c stack.c vm.c words.c
SRCS=			${BASE_SRCS} sysdep.c softcore.c
CLEANFILES=		softcore.c testmain testmain.o
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=		-mno-fp-regs
.endif
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=		-mpreferred-stack-boundary=2
.endif
.ifmake testmain
CFLAGS+=			-DTESTMAIN -D_TESTMAIN
SRCS+=				testmain.c
PROG=			testmain
.include <bsd.prog.mk>
.else
LIB=			ficl
INTERNALLIB=		yes
INTERNALSTATICLIB=	yes
NOPROFILE=		yes
SRCS+=			loader.c
.include <bsd.lib.mk>
.endif

# Standard softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.PATH:		${.CURDIR}/softwords
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}/../common -DFICL_TRACE

softcore.c:	${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} \
	    | awk -f softcore.awk -v datestamp="`LC_ALL=C date`") > ${.TARGET}


