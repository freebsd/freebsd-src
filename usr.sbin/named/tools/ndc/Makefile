MAN8=	ndc.8
CLEANFILES+=ndc

PIDDIR=/var/run
INDOT=
PS=ps
IOT=ABRT
DESTSBIN=${DESTDIR}${BINDIR}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 555 \
	    ndc ${DESTDIR}${BINDIR}

all: ndc

ndc: ndc.sh Makefile
	sed -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%PIDDIR%|${PIDDIR}|" \
	    -e "s|%PS%|${PS}|" \
	    -e "s|%IOT%|${IOT}|" \
	    < ${.CURDIR}/ndc.sh > ndc

.include "${.CURDIR}/../../../Makefile.inc"
.include <bsd.prog.mk>
