# Really quick and evil Makefile for building all the tests.  I wish that bmake was friendlier to
# the concept of multiple progs/libs in the same directory.
# $Id: Makefile,v 1.1.2.1 1996/01/02 06:42:45 jkh Exp $

PROGS=	msg yesno prgbox gauge dselect fselect text menu1 menu2 menu3 \
	input1 check1 check2 check3 radio1 radio2 radio3

CFLAGS+= -Wall -Wstrict-prototypes
LDFLAGS += -ldialog -lncurses -lmytinfo

all: ${PROGS}

clean:
	rm -f ${PROGS}
	
msg:	${.CURDIR}/msg.c
	${CC} ${.CURDIR}/msg.c -o msg ${LDFLAGS}

yesno:	${.CURDIR}/yesno.c
	${CC} ${.CURDIR}/yesno.c -o yesno ${LDFLAGS}

prgbox:	${.CURDIR}/prgbox.c
	${CC} ${.CURDIR}/prgbox.c -o prgbox ${LDFLAGS}

gauge:	${.CURDIR}/gauge.c
	${CC} ${.CURDIR}/gauge.c -o gauge ${LDFLAGS}

dselect: ${.CURDIR}/dselect.c
	${CC} ${.CURDIR}/dselect.c -o dselect ${LDFLAGS}

fselect: ${.CURDIR}/fselect.c
	${CC} ${.CURDIR}/fselect.c -o fselect ${LDFLAGS}

text:	${.CURDIR}/text.c
	${CC} ${.CURDIR}/text.c -o text ${LDFLAGS}

menu1:	${.CURDIR}/menu1.c
	${CC} ${.CURDIR}/menu1.c -o menu1 ${LDFLAGS}

menu2:	${.CURDIR}/menu2.c
	${CC} ${.CURDIR}/menu2.c -o menu2 ${LDFLAGS}

menu3:	${.CURDIR}/menu3.c
	${CC} ${.CURDIR}/menu3.c -o menu3 ${LDFLAGS}

input1:	${.CURDIR}/input1.c
	${CC} ${.CURDIR}/input1.c -o input1 ${LDFLAGS}

check1:	${.CURDIR}/check1.c
	${CC} ${.CURDIR}/check1.c -o check1 ${LDFLAGS}

check2:	${.CURDIR}/check2.c
	${CC} ${.CURDIR}/check2.c -o check2 ${LDFLAGS}

check3: ${.CURDIR}/check3.c
	${CC} ${.CURDIR}/check3.c -o check3 ${LDFLAGS}

radio1: ${.CURDIR}/radio1.c
	${CC} ${.CURDIR}/radio1.c -o radio1 ${LDFLAGS}

radio2: ${.CURDIR}/radio2.c
	${CC} ${.CURDIR}/radio2.c -o radio2 ${LDFLAGS}

radio3: ${.CURDIR}/radio3.c
	${CC} ${.CURDIR}/radio3.c -o radio3 ${LDFLAGS}

.include <bsd.prog.mk>
