# $FreeBSD$

MAINTAINER=	des@freebsd.org
LIB=		fetch
CFLAGS+=	-I. -Wall -pedantic
.if !defined(DEBUG)
CFLAGS+=	-DNDEBUG
.endif
SRCS=		fetch.c common.c ftp.c http.c file.c fetch_err.c \
		fetch_err.h ftperr.h httperr.h
INCS=		fetch.h ${.OBJDIR}/fetch_err.h
MAN3=		fetch.3
CLEANFILES=	fetch_err.c fetch_err.h ftperr.h httperr.h

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

hdrs: fetch_err.h

.ORDER: fetch_err.c fetch_err.h
fetch_err.c fetch_err.h: fetch_err.et
	compile_et ${.ALLSRC}

.include <bsd.lib.mk>
