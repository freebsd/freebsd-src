# $FreeBSD: src/sys/boot/ficl/Makefile,v 1.17.2.2 2000/07/06 23:44:58 obrien Exp $
#
.PATH:			${.CURDIR}/${MACHINE_ARCH}
BASE_SRCS=		dict.c ficl.c math64.c stack.c vm.c words.c
SRCS=			${BASE_SRCS} sysdep.c softcore.c
CLEANFILES=		softcore.c testmain testmain.o
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=		-mno-fp-regs
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
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} | awk -f softcore.awk) > ${.TARGET}


