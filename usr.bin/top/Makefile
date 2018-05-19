# $FreeBSD$

PROG=	top
SRCS=	commands.c display.c machine.c screen.c top.c \
	username.c utils.c version.c 
SRCS+=	sigdesc.h top.local.h
CFLAGS+= -DHAVE_GETOPT -DHAVE_STRERROR -DORDER -I ${.OBJDIR}
MAN=	top.1

WARNS?=	1

LIBADD=	ncursesw m kvm jail

CLEANFILES= sigdesc.h
SIGNAL_H= ${SRCTOP}/sys/sys/signal.h
sigdesc.h: sigconv.awk ${SIGNAL_H}
	awk -f ${SRCTOP}/usr.bin/top/sigconv.awk < ${SIGNAL_H} > ${.TARGET}

.SUFFIXES: .xs .x .hs .h
.xs.x .hs.h:
	@${ECHO} Making ${.TARGET} from ${.IMPSRC}
	@sed -e's,%LoadMax%,5.0,g' \
	-e's,%TableSize%,20011,g' \
	-e's,%NominalTopn%,18,g' \
	-e's,%topn%,-1,g' \
	-e's,%delay%,2,g' \
	-e's,%random%,1,g' \
	${.IMPSRC} > ${.TARGET}

.include <bsd.prog.mk>
