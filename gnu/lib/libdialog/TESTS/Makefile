# Really quick and evil Makefile for building all the tests.  I wish that bmake was friendlier to
# the concept of multiple progs/libs in the same directory.
# $Id: Makefile,v 1.7 1997/02/22 15:42:37 peter Exp $

PROGS=	msg yesno prgbox gauge dselect fselect text menu1 menu2 menu3 \
	input1 input2 check1 check2 check3 radio1 radio2 radio3 \
	ftree1 ftree2 tree

CFLAGS+= -Wall -Wstrict-prototypes
LDFLAGS += -ldialog -lncurses -lmytinfo

all: ${PROGS}

clean:
	rm -f ${PROGS}
	
.include <bsd.prog.mk>
