# $FreeBSD$

PROG=	asn1_compile
NO_MAN=
LIBROKEN_A=	${.OBJDIR}/../../lib/libroken/libroken.a
LDADD=	${LIBROKEN_A} ${LIBVERS}
DPADD=	${LIBROKEN_A} ${LIBVERS}

SRCS=	asn1_queue.h \
	asn1parse.y \
	der.h \
	gen.c \
	gen_copy.c \
	gen_decode.c \
	gen_encode.c \
	gen_free.c \
	gen_glue.c \
	gen_length.c \
	gen_locl.h \
	gen_seq.c \
	gen_template.c \
	hash.c \
	hash.h \
	lex.l \
	lex.h \
	main.c \
	asn1-template.h \
	roken.h \
	symbol.c \
	symbol.h

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/asn1 -I.

CLEANFILES= roken.h

roken.h:
	 make-roken > ${.TARGET}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/asn1
