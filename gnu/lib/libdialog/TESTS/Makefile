# Really quick and evil Makefile for building all the tests.  I wish that
# bmake was friendlier to the concept of multiple progs/libs in the same
# directory.
#
# $FreeBSD: src/gnu/lib/libdialog/TESTS/Makefile,v 1.11.36.1 2010/02/10 00:26:20 kensmith Exp $

PROGS=	msg yesno prgbox gauge dselect fselect text menu1 menu2 menu3 \
	input1 input2 check1 check2 check3 radio1 radio2 radio3 \
	ftree1 ftree2 tree

WARNS?=	2
CFLAGS+= -Wall -Wstrict-prototypes
LDFLAGS+= -ldialog

all: ${PROGS}

clean:
	rm -f ${PROGS}
	
.include <bsd.prog.mk>
