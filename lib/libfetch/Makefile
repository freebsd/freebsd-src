# $FreeBSD$

MAINTAINER=	des@freebsd.org
LIB=		fetch
CFLAGS+=	-I. -Wall -pedantic
CFLAGS+=	-DINET6
.if !defined(DEBUG)
CFLAGS+=	-DNDEBUG
.endif
SRCS=		fetch.c common.c ftp.c http.c file.c \
		ftperr.h httperr.h
INCS=		fetch.h
MAN3=		fetch.3
CLEANFILES=	ftperr.h httperr.h

SHLIB_MAJOR=    2
SHLIB_MINOR=	0

ftperr.h: ftp.errors
	@echo "static struct fetcherr _ftp_errlist[] = {" > ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown FTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

httperr.h: http.errors
	@echo "static struct fetcherr _http_errlist[] = {" > ${.TARGET}
	@cat ${.ALLSRC} \
	  | grep -v ^# \
	  | sort \
	  | while read NUM CAT STRING; do \
	    echo "    { $${NUM}, FETCH_$${CAT}, \"$${STRING}\" },"; \
	  done >> ${.TARGET}
	@echo "    { -1, FETCH_UNKNOWN, \"Unknown HTTP error\" }" >> ${.TARGET}
	@echo "};" >> ${.TARGET}

.for MP in fetchFreeURL fetchGet fetchGetFTP fetchGetFile fetchGetHTTP \
fetchGetURL fetchList fetchListFTP fetchListFile fetchListHTTP fetchListURL \
fetchMakeURL fetchParseURL fetchPut fetchPutFTP fetchPutFile fetchPutHTTP \
fetchPutURL fetchStat fetchStatFTP fetchStatFile fetchStatHTTP fetchStatURL \
fetchXGet fetchXGetFTP fetchXGetFile fetchXGetHTTP fetchXGetURL
MLINKS+= fetch.3 ${MP}.3
.endfor

.include <bsd.lib.mk>
