# $FreeBSD$

PROG=	top
SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c
SRCS+=	sigdesc.h top.local.h
CFLAGS+= -I ${.OBJDIR}
MAN=	top.1

WARNS?=	1

LIBADD=	ncursesw m kvm jail

CLEANFILES= sigdesc.h
SIGNAL_H= ${SRCTOP}/sys/sys/signal.h
sigdesc.h: sigconv.awk ${SIGNAL_H}
	awk -f ${SRCTOP}/usr.bin/top/sigconv.awk < ${SIGNAL_H} > ${.TARGET}

.include <bsd.prog.mk>
