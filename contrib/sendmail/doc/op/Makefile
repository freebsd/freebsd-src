#	$Id: Makefile,v 8.7.8.4 2001/07/01 18:34:56 gshapiro Exp $

DIR=		smm/08.sendmailop
SRCS=		op.me
OBJS=		op.ps
MACROS=		-me
ROFF_CMD=	groff
PIC_CMD=	pic
EQN_CMD=	eqn
UL_CMD=		ul
PIC=		${PIC_CMD} -C
EQNASCII=	${EQN_CMD} -C -Tascii
EQNPS=		${EQN_CMD} -C -Tps
ROFFASCII=	${ROFF_CMD} -Tascii ${MACROS}
ROFFPS=		${ROFF_CMD} -Tps -mps ${MACROS}
ULASCII=	${UL_CMD} -t dumb

all: ${OBJS}

op.ps: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNPS} | ${ROFFPS} > $@

op.txt: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNASCII} | ${ROFFASCII} | ${ULASCII} > $@

clean:
	rm -f op.ps op.txt

install: ${OBJS}
