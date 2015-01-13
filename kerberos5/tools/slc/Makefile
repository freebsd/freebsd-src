# $FreeBSD$

PROG=	slc
LIBROKEN_A=     ${.OBJDIR}/../../lib/libroken/libroken.a
LIBADD=	vers
LDADD=  ${LIBROKEN_A}
DPADD=  ${LIBROKEN_A}
MAN=

SRCS=	roken.h \
	slc-gram.y \
	slc-lex.l

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I${KRB5DIR}/lib/vers -I.

CLEANFILES= roken.h slc-gram.c slc-lex.c

roken.h:
	${MAKE_ROKEN} > ${.TARGET}

#	${.OBJDIR}/../make-roken/make-roken > ${.TARGET}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/sl
