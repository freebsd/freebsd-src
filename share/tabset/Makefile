#	@(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	3101 9837 aa aed512 beehive diablo dtc382 hp700-wy ibm3101 std \
	stdcrt tandem653 teleray vt100 vt100-w wyse-adds xerox1720 xerox1730 \
	xerox1730-lm zenith29

depend lint tags:

clean cleandir:
	rm -f ${FILES}

all:
	for i in ${FILES} ; do uudecode < ${.CURDIR}/$$i.uu ; done
	
install:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/tabset

.include <bsd.prog.mk>
