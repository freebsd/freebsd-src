# $FreeBSD$
#
LIB=			ficl
NOPROFILE=		yes
INTERNALLIB=		yes
INTERNALSTATICLIB=	yes
BASE_SRCS=		dict.c ficl.c math64.c stack.c vm.c words.c
SRCS=			${BASE_SRCS} ${MACHINE_ARCH}/sysdep.c softcore.c
CLEANFILES=		softcore.c testmain

# Standard softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.PATH:		${.CURDIR}/softwords
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} -DFICL_TRACE

softcore.c:	${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} | awk -f softcore.awk) > ${.TARGET}

.include <bsd.lib.mk>

CFLAGS+=	-DTESTMAIN

testmain:      ${.CURDIR}/testmain.c ${OBJS}
	cc -o ${.TARGET} ${CFLAGS} ${.CURDIR}/testmain.c ${OBJS}

