# $FreeBSD: src/sys/i386/boot/Makefile.inc,v 1.6 1999/08/28 00:43:08 peter Exp $

BINDIR?=	/usr/mdec
CFLAGS+=	-aout
.if exists(${.CURDIR}/../../../../include)
CFLAGS+=	-nostdinc -I${.CURDIR}/../../../../include
.endif
CFLAGS+=	-I${.CURDIR}/../../.. -I${.OBJDIR}
CLEANFILES+=	machine

.if defined(SRCS)
${SRCS:M*.[sS]:R:S/$/.o/g} ${SRCS:M*.c:R:S/$/.o/g}: machine
${SRCS:M*.cc:R:S/$/.o/g}   ${SRCS:M*.C:R:S/$/.o/g}: machine
${SRCS:M*.cxx:R:S/$/.o/g}  ${SRCS:N*.h:R:S/$/.o/g}: machine
.endif
.if defined(OBJS)
${OBJS}: machine
.endif

beforedepend: machine
machine:
	ln -s ${.CURDIR}/../../include ${.OBJDIR}/machine
