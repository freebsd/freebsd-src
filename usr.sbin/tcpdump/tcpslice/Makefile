#	@(#)Makefile	0.1 (RWGrimes) 3/24/93

PROG=	tcpslice
CFLAGS+=-Wall -I.
MAN1=	tcpslice.1
SRCS=	version.c tcpslice.c gwtm2secs.c search.c util.c
CLEANFILES+=	version.c version.h
DPADD+=	${LIBPCAP}
LDADD+= -lpcap

DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump

version.c version.h: $(DISTDIR}/VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' ${DISTDIR}/VERSION > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' ${DISTDIR}/VERSION` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h

.include <bsd.prog.mk>
