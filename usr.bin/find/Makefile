.include <src.opts.mk>

PACKAGE=	runtime

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c printf.c \
	getdate.y
YFLAGS=
CFLAGS.clang+=	-Werror=undef

NO_WMISSING_VARIABLE_DECLARATIONS=

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
