# $FreeBSD$

PROG=	asn1_compile
MAN=
LIBROKEN_A=	${.OBJDIR}/../../lib/libroken/libroken.a
LDADD=	${LIBROKEN_A} ${LIBVERS}
DPADD=	${LIBROKEN_A} ${LIBVERS}

SRCS=	\
	asn1parse.y \
	gen.c \
	gen_copy.c \
	gen_decode.c \
	gen_encode.c \
	gen_free.c \
	gen_glue.c \
	gen_length.c \
	gen_seq.c \
	gen_template.c \
	hash.c \
	lex.l \
	main.c \
	roken.h \
	symbol.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/asn1 -I.

CLEANFILES= roken.h lex.c parse.c

NO_PIE=	yes

roken.h:
	 make-roken > ${.TARGET}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/asn1
