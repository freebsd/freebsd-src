#	$Id: Makefile,v 8.7 2000/02/01 08:21:47 gshapiro Exp $

DIR=	smm/08.sendmailop
SRCS=	op.me
OBJS=	op.ps
MACROS=	-me
ROFF_CMD=	groff
PIC_CMD=	pic
EQN_CMD=	eqn
PIC=		${PIC_CMD} -C
EQN=		${EQN_CMD} -C -Tps
ROFF=		${ROFF_CMD} -Tps -mps ${MACROS}

all: ${OBJS}

${OBJS}: ${SRCS}
	rm -f $@
	${PIC} ${SRCS} | ${EQN} | ${ROFF} > $@

clean:
	rm -f ${OBJS}

install: ${OBJS}
