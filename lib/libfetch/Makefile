#	$Id: Makefile,v 1.7 1998/11/06 22:14:08 des Exp $

LIB=		fetch
CFLAGS+=	-I. -Wall -pedantic
.if !defined(DEBUG)
CFLAGS+=	-DNDEBUG
.endif
SRCS=		fetch.c common.c ftp.c http.c file.c fetch_err.c
DPSRCS=		ftperr.inc httperr.inc fetch_err.c fetch_err.h
MAN3=		fetch.3
CLEANFILES=	${DPSRCS}

SHLIB_MAJOR=    1
SHLIB_MINOR=	0

beforedepend: ${DPSRCS}

beforeinstall: fetch.h fetch_err.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/fetch.h \
		${DESTDIR}/usr/include
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 fetch_err.h \
		${DESTDIR}/usr/include

ftperr.inc: ftp.errors
	@echo "static struct fetcherr _ftp_errlist[] = {" > ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}


httperr.inc: http.errors
	@echo "static struct fetcherr _http_errlist[] = {" > ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

fetch_err.c fetch_err.h: fetch_err.et
	compile_et ${.ALLSRC}

.include <bsd.lib.mk>

.if !exists(${DEPENDFILE})
${OBJS} ${POBJS} ${SOBJS}: ${DPSRCS}
.endif
