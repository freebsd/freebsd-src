# $FreeBSD$
#
LIB=			ficl
NOPROFILE=		yes
INTERNALLIB=		yes
INTERNALSTATICLIB=	yes
BASE_SRCS=		dict.c ficl.c math64.c stack.c sysdep.c vm.c words.c
SRCS=			${BASE_SRCS} softcore.c
CLEANFILES=		softcore.c testmain

# Standard softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.PATH:		${.CURDIR}/softwords
CFLAGS+=	-I${.CURDIR} -DFICL_TRACE

softcore.c:	${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} | awk -f softcore.awk) > ${.TARGET}

.include <bsd.lib.mk>

testmain:      ${.CURDIR}/testmain.c ${SRCS}
	@for i in ${BASE_SRCS}; do echo $${i}... ; \
	  ${CC} -c ${CFLAGS} -DTESTMAIN ${.CURDIR}/$${i}; done
	@echo softdep.c...
	@${CC} -c ${CFLAGS} -D_TESTMAIN softcore.c
	cc -o ${.TARGET} ${.CURDIR}/testmain.c ${OBJS}

