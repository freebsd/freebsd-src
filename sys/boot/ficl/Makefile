# $Id: Makefile,v 1.5 1998/11/05 07:27:55 jkh Exp $
#
LIB=			ficl
NOPROFILE=		yes
INTERNALLIB=		yes
INTERNALSTATICLIB=	yes
BASE_SRCS=		dict.c ficl.c math64.c stack.c sysdep.c vm.c words.c
SRCS=			${BASE_SRCS} softcore.c
CLEANFILES=		softcore.c testmain

# Standard softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.PATH:		${.CURDIR}/softwords
CFLAGS+=	-I${.CURDIR}

softcore.c:	${SOFTWORDS} softcore.pl
	(cd ${.CURDIR}/softwords; perl softcore.pl ${SOFTWORDS}) > ${.TARGET}

.include <bsd.lib.mk>

testmain:      ${.CURDIR}/testmain.c ${SRCS}
	@for i in ${BASE_SRCS}; do echo $${i}... ; \
	  ${CC} -c ${CFLAGS} -DTESTMAIN ${.CURDIR}/$${i}; done
	@echo softdep.c...
	@${CC} -c ${CFLAGS} -D_TESTMAIN softcore.c
	cc -o ${.TARGET} ${.CURDIR}/testmain.c ${OBJS}

