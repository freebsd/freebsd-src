#
# Makefile for INFO files
#

INFOFILES= dir

install:
	${INSTALL} -c -g ${BINGRP} -o ${BINOWN} -m 444 ${INFOFILES} \
		${DESTDIR}${INFODIR}

clean cleandir obj:
	@echo -n

.include <bsd.prog.mk>
