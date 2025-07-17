.include <src.opts.mk>

PROG=	locale
CFLAGS+= -I${SRCTOP}/lib/libc/locale
LIBADD+= sbuf

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
