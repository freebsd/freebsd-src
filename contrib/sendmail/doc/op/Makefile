#	$Id: Makefile,v 8.7.8.1 2001/01/04 18:55:46 gshapiro Exp $

DIR=		smm/08.sendmailop
SRCS=		op.me
OBJS=		op.ps
MACROS=		-me
ROFF_CMD=	groff
PIC_CMD=	pic
EQN_CMD=	eqn
PIC=		${PIC_CMD} -C
EQNASCII=	${EQN_CMD} -C -Tascii
EQNPS=		${EQN_CMD} -C -Tps
ROFFASCII=	${ROFF_CMD} -Tascii ${MACROS}
ROFFPS=		${ROFF_CMD} -Tps -mps ${MACROS}

all: ${OBJS}

op.ps: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNPS} | ${ROFFPS} > $@

op.txt: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNASCII} | ${ROFFASCII} > $@

clean:
	rm -f op.ps op.txt

install: ${OBJS}
