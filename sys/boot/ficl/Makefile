# $FreeBSD: src/sys/boot/ficl/Makefile,v 1.17 2000/01/28 03:07:25 dcs Exp $
#
.PATH:			${.CURDIR}/${MACHINE_ARCH}
BASE_SRCS=		dict.c ficl.c math64.c stack.c vm.c words.c
SRCS=			${BASE_SRCS} sysdep.c softcore.c
CLEANFILES=		softcore.c testmain testmain.o
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
.include <bsd.lib.mk>
.endif

# Standard softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.PATH:		${.CURDIR}/softwords
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} -DFICL_TRACE

softcore.c:	${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} | awk -f softcore.awk) > ${.TARGET}


