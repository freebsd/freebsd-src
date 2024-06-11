.include <src.opts.mk>

LIB=	diff
INTERNALLIB=	# API not published or supported.

.PATH: ${SRCTOP}/contrib/libdiff/compat
.PATH: ${SRCTOP}/contrib/libdiff/lib

SRCS=	diff_atomize_text.c diff_main.c diff_myers.c \
	diff_patience.c diff_output.c diff_output_plain.c \
	diff_output_unidiff.c diff_output_edscript.c recallocarray.c

WARNS=
CFLAGS+= -I${SRCTOP}/contrib/libdiff/compat/include
CFLAGS+= -I${SRCTOP}/contrib/libdiff/include

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
