# Really quick and evil Makefile for building all the tests.  I wish that bmake was friendlier to
# the concept of multiple progs/libs in the same directory.
# $Id$

PROGS=	msg yesno prgbox gauge dselect fselect text menu1 menu2 menu3 \
	input1 check1 check2 check3 radio1 radio2 radio3

CFLAGS+= -Wall -Wstrict-prototypes
LDFLAGS += -ldialog -lncurses -lmytinfo

all: ${PROGS}

clean:
	rm -f ${PROGS}
	
msg:	msg.c
	${CC} msg.c -o msg ${LDFLAGS}

yesno:	yesno.c
	${CC} yesno.c -o yesno ${LDFLAGS}

prgbox:	prgbox.c
	${CC} prgbox.c -o prgbox ${LDFLAGS}

gauge:	gauge.c
	${CC} gauge.c -o gauge ${LDFLAGS}

dselect: dselect.c
	${CC} dselect.c -o dselect ${LDFLAGS}

fselect: fselect.c
	${CC} fselect.c -o fselect ${LDFLAGS}

text:	text.c
	${CC} text.c -o text ${LDFLAGS}

menu1:	menu1.c
	${CC} menu1.c -o menu1 ${LDFLAGS}

menu2:	menu2.c
	${CC} menu2.c -o menu2 ${LDFLAGS}

menu3:	menu3.c
	${CC} menu3.c -o menu3 ${LDFLAGS}

input1:	input1.c
	${CC} input1.c -o input1 ${LDFLAGS}

check1:	check1.c
	${CC} check1.c -o check1 ${LDFLAGS}

check2:	check2.c
	${CC} check2.c -o check2 ${LDFLAGS}

check3: check3.c
	${CC} check3.c -o check3 ${LDFLAGS}

radio1: radio1.c
	${CC} radio1.c -o radio1 ${LDFLAGS}

radio2: radio2.c
	${CC} radio2.c -o radio2 ${LDFLAGS}

radio3: radio3.c
	${CC} radio3.c -o radio3 ${LDFLAGS}
