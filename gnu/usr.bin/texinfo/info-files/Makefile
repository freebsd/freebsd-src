#
# Makefile for INFO files
#

INFOFILES+= dir info.info makeinfo.info texi.info texi.info-1 texi.inf-2  
INFOFILES+= texi.info-3 texi.info-4 texi.info-5 texi.info-6 texi.info-7
INFOFILES+= texi.info-8 texi.info-9 texi.info-10 texi.info-11

install:
	mkdir -p ${INFODIR} 
	install -g ${BINGRP} -o ${BINOWN} -m 444 ${INFOFILES} ${INFODIR}

clean:
cleandir:
obj:

.include <bsd.prog.mk>

