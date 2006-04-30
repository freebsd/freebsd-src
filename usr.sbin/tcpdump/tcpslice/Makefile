#	@(#)Makefile	0.1 (RWGrimes) 3/24/93
# $FreeBSD$

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump

PROG=	tcpslice
SRCS=	gwtm2secs.c search.c tcpslice.c util.c version.c version.h
CLEANFILES=	version.c version.h

CFLAGS+= -I.

DPADD=	${LIBPCAP}
LDADD=	-lpcap

.ORDER: version.c version.h
version.c version.h: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	    sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' ${TCPDUMP_DISTDIR}/VERSION` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h

.include <bsd.prog.mk>
