# $FreeBSD$

PROG=	asn1_compile
NO_MAN=

SRCS=	gen.c \
	gen_copy.c \
	gen_decode.c \
	gen_encode.c \
	gen_free.c \
	gen_glue.c \
	gen_length.c \
	hash.c \
	emalloc.c \
	main.c \
	symbol.c \
	getarg.c \
	warnerr.c \
	lex.l \
	parse.y \
	print_version.c \
	print_version.h \
	roken.h \
	get_window_size.c \
	strupr.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/asn1 -I.

CLEANFILES= print_version.h roken.h

print_version.h: ../make-print-version/make-print-version
	../make-print-version/make-print-version ${.TARGET}

../make-print-version/make-print-version:
	cd ${.CURDIR}/../make-print-version && ${MAKE}

roken.h: ../make-roken/make-roken
	../make-roken/make-roken > ${.TARGET}

../make-roken/make-roken:
	cd ${.CURDIR}/../make-roken && ${MAKE}

.include <bsd.prog.mk>

# There are two print_version.c's, the one we need is in ${KRB5DIR}/lib/vers,
# so the order of paths is important here.
.PATH: ${KRB5DIR}/lib/vers ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/asn1
