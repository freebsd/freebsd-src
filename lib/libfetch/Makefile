LIB=		fetch
CFLAGS+=	-I${.CURDIR} -Wall
SRCS=		fetch.c ftp.c http.c file.c base64.c
MAN3=		fetch.3
CLEANFILES+=	ftperr.c httperr.c

SHLIB_MAJOR=    1
SHLIB_MINOR=	0

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/fetch.h \
		${DESTDIR}/usr/include

ftperr.c:	ftp.errors
	@echo "struct ftperr {" \ >>  ${.TARGET}
	@echo "    const int num;" \ >>  ${.TARGET}
	@echo "    const char *string;" \ >>  ${.TARGET}
	@echo "};" \ >>  ${.TARGET}
	@echo "static struct ftperr _ftp_errlist[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "    { $${NUM}, \"$${NUM} $${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { 0, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

httperr.c:	http.errors
	@echo "struct httperr {" \ >>  ${.TARGET}
	@echo "    const int num;" \ >>  ${.TARGET}
	@echo "    const char *string;" \ >>  ${.TARGET}
	@echo "};" \ >>  ${.TARGET}
	@echo "static struct httperr _http_errlist[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "    { $${NUM}, \"$${NUM} $${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { 0, \"Unknown HTTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

.include <bsd.lib.mk>
