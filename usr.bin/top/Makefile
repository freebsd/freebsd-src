# $FreeBSD$

.include <src.opts.mk>

PROG=	top
SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c
MAN=	top.1

.if ${COMPILER_TYPE} == "gcc"
.if ${COMPILER_VERSION} >= 50000
CFLAGS.gcc=-Wno-error=discarded-qualifiers -Wno-error=incompatible-pointer-types
.else #base gcc
NO_WERROR=
.endif
.endif
CFLAGS.clang=-Wno-error=incompatible-pointer-types-discards-qualifiers

LIBADD=	ncursesw m kvm jail util sbuf
.include <bsd.prog.mk>
