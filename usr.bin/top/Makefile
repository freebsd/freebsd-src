# $FreeBSD$

.include <src.opts.mk>

PROG=	top
SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c sigdesc.h
CFLAGS+= -I ${.OBJDIR}
MAN=	top.1

WARNS?=	6

.if ${COMPILER_TYPE} == "gcc"
.if ${COMPILER_VERSION} >= 50000
CFLAGS.gcc=-Wno-error=cast-align -Wno-error=cast-qual -Wno-error=discarded-qualifiers -Wno-error=incompatible-pointer-types \
	-Wno-error=maybe-uninitialized
.else #base gcc
NO_WERROR=
.endif
.endif
CFLAGS.clang=-Wno-error=incompatible-pointer-types-discards-qualifiers -Wno-error=cast-qual -Wno-error=cast-align

LIBADD=	ncursesw m kvm jail

CLEANFILES= sigdesc.h
SIGNAL_H= ${SRCTOP}/sys/sys/signal.h
sigdesc.h: sigconv.awk ${SIGNAL_H}
	awk -f ${SRCTOP}/usr.bin/top/sigconv.awk < ${SIGNAL_H} > ${.TARGET}

.include <bsd.prog.mk>
