#	$Id$

LIB=		fetch
CFLAGS+=	-I. -Wall -pedantic -DNDEBUG
SRCS=		fetch.c ftp.c http.c file.c
DPSRCS=		ftperr.c httperr.c
MAN3=		fetch.3
CLEANFILES=	${DPSRCS}

SHLIB_MAJOR=    1
SHLIB_MINOR=	0

beforedepend: ${DPSRCS}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/fetch.h \
		${DESTDIR}/usr/include

ftperr.c: ftp.errors
	@echo "struct ftperr {" \ >  ${.TARGET}
	@echo "    const int num;" \ >>  ${.TARGET}
	@echo "    const char *string;" \ >>  ${.TARGET}
	@echo "};" \ >>  ${.TARGET}
	@echo "static struct ftperr _ftp_errlist[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "    { $${NUM}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

httperr.c: http.errors
	@echo "struct httperr {" \ >  ${.TARGET}
	@echo "    const int num;" \ >>  ${.TARGET}
	@echo "    const char *string;" \ >>  ${.TARGET}
	@echo "};" \ >>  ${.TARGET}
	@echo "static struct httperr _http_errlist[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "    { $${NUM}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, \"Unknown HTTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

.include <bsd.lib.mk>

.if !exists(${DEPENDFILE})
${OBJS} ${POBJS} ${SOBJS}: ${DPSRCS}
.endif
