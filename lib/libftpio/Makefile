LIB=		ftpio
CFLAGS+=	-I${.CURDIR} -Wall
SRCS=		ftpio.c ftperr.c
MAN3=		ftpio.3
CLEANFILES+=	ftperr.c

beforeinstall:
	cd ${.CURDIR}; cmp -s ftpio.h ${DESTDIR}/usr/include/ftpio.h || \
	  ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ftpio.h ${DESTDIR}/usr/include

ftperr.c:	ftp.errors
	@echo '#include <stdio.h>' > ${.TARGET}
	@echo '#include "ftpio.h"' >> ${.TARGET}
	@echo "struct ftperr ftpErrList[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "  { $${NUM}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "};" >> ${.TARGET}
	@echo -n "int const ftpErrListLength = " >> ${.TARGET}
	@echo "sizeof(ftpErrList) / sizeof(*ftpErrList);" >> ${.TARGET}

.include <bsd.lib.mk>
