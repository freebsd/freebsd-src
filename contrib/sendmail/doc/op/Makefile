#	$Id: Makefile,v 8.14 2002/01/07 22:24:36 gshapiro Exp $

DIR=		smm/08.sendmailop
SRCS=		op.me
OBJS=		op.ps
MACROS=		-me
ROFF_CMD=	groff
PIC_CMD=	pic
EQN_CMD=	eqn
UL_CMD=		ul
PS2PDF_CMD=	ps2pdf
PIC=		${PIC_CMD} -C
EQNASCII=	${EQN_CMD} -C -Tascii
EQNPS=		${EQN_CMD} -C -Tps
ROFFASCII=	${ROFF_CMD} -Tascii ${MACROS}
ROFFPS=		${ROFF_CMD} -Tps -mps ${MACROS}
ULASCII=	${UL_CMD} -t dumb
PS2PDF=		${PS2PDF_CMD}

all: ${OBJS}

op.ps: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNPS} | ${ROFFPS} > $@

op.txt: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQNASCII} | ${ROFFASCII} | ${ULASCII} > $@

op.pdf: op.ps
	rm -f $@
	${PS2PDF} op.ps op.pdf

clean:
	rm -f op.ps op.txt op.pdf

install: ${OBJS}
