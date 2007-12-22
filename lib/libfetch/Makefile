# $FreeBSD$

.include <bsd.own.mk>

LIB=		fetch
CFLAGS+=	-I.
SRCS=		fetch.c common.c ftp.c http.c file.c \
		ftperr.h httperr.h
INCS=		fetch.h
MAN=		fetch.3
CLEANFILES=	ftperr.h httperr.h

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DINET6
.endif

.if ${MK_OPENSSL} != "no"
CFLAGS+=	-DWITH_SSL
DPADD=		${LIBSSL} ${LIBCRYPTO}
LDADD=		-lssl -lcrypto
.endif

CFLAGS+=	-DFTP_COMBINE_CWDS

CSTD?=		c99
WARNS?=		2

SHLIB_MAJOR=    5

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

MLINKS+= fetch.3 fetchFreeURL.3
MLINKS+= fetch.3 fetchGet.3
MLINKS+= fetch.3 fetchGetFTP.3
MLINKS+= fetch.3 fetchGetFile.3
MLINKS+= fetch.3 fetchGetHTTP.3
MLINKS+= fetch.3 fetchGetURL.3
MLINKS+= fetch.3 fetchList.3
MLINKS+= fetch.3 fetchListFTP.3
MLINKS+= fetch.3 fetchListFile.3
MLINKS+= fetch.3 fetchListHTTP.3
MLINKS+= fetch.3 fetchListURL.3
MLINKS+= fetch.3 fetchMakeURL.3
MLINKS+= fetch.3 fetchParseURL.3
MLINKS+= fetch.3 fetchPut.3
MLINKS+= fetch.3 fetchPutFTP.3
MLINKS+= fetch.3 fetchPutFile.3
MLINKS+= fetch.3 fetchPutHTTP.3
MLINKS+= fetch.3 fetchPutURL.3
MLINKS+= fetch.3 fetchStat.3
MLINKS+= fetch.3 fetchStatFTP.3
MLINKS+= fetch.3 fetchStatFile.3
MLINKS+= fetch.3 fetchStatHTTP.3
MLINKS+= fetch.3 fetchStatURL.3
MLINKS+= fetch.3 fetchXGet.3
MLINKS+= fetch.3 fetchXGetFTP.3
MLINKS+= fetch.3 fetchXGetFile.3
MLINKS+= fetch.3 fetchXGetHTTP.3
MLINKS+= fetch.3 fetchXGetURL.3

.include <bsd.lib.mk>
