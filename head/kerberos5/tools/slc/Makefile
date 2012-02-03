# $FreeBSD$

PROG=	slc
NO_MAN=

SRCS=	get_window_size.c \
	getarg.c \
	roken.h \
	slc-gram.y \
	slc-lex.l \
	slc.h \
	strupr.c

SRCS+=	print_version.c \
	print_version.h

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I${KRB5DIR}/lib/vers -I.

CLEANFILES= print_version.h roken.h

print_version.h: ../make-print-version/make-print-version
	../make-print-version/make-print-version ${.TARGET}

../make-print-version/make-print-version: .PHONY
	cd ${.CURDIR}/../make-print-version && ${MAKE}

roken.h: ../make-roken/make-roken
	../make-roken/make-roken > ${.TARGET}

../make-roken/make-roken: .PHONY
	cd ${.CURDIR}/../make-roken && ${MAKE}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/vers ${KRB5DIR}/lib/roken ${KRB5DIR}/lib/sl
