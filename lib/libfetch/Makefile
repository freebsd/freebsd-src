#	$Id: Makefile,v 1.5 1998/08/17 20:39:09 bde Exp $

LIB=		fetch
CFLAGS+=	-I. -Wall -pedantic
.if !defined(DEBUG)
CFLAGS+=	-DNDEBUG
.endif
SRCS=		fetch.c common.c ftp.c http.c file.c
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
	@echo "static struct fetcherr _ftp_errlist[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "    { $${NUM}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}
	@echo "#define _ftp_errstring(n) _fetch_errstring(_ftp_errlist, n)" >> ${.TARGET}
	@echo "#define _ftp_seterr(n) _fetch_seterr(_ftp_errlist, n)" >> ${.TARGET}


httperr.c: http.errors
	@echo "static struct fetcherr _http_errlist[] = {" \ >>  ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM STRING; do \
	    echo "    { $${NUM}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, \"Unknown HTTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}
	@echo "#define _http_errstring(n) _fetch_errstring(_http_errlist, n)" >> ${.TARGET}
	@echo "#define _http_seterr(n) _fetch_seterr(_http_errlist, n)" >> ${.TARGET}

.include <bsd.lib.mk>

.if !exists(${DEPENDFILE})
${OBJS} ${POBJS} ${SOBJS}: ${DPSRCS}
.endif
