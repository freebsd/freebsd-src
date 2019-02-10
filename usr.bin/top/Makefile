# $FreeBSD$

.include <src.opts.mk>

PROG=	top
SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c
MAN=	top.1

.if ${COMPILER_TYPE} == "gcc" && ${COMPILER_VERSION} < 50000
NO_WERROR=
.endif

LIBADD=	ncursesw m kvm jail util sbuf
.include <bsd.prog.mk>
