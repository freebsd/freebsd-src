#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.3 1995/01/14 09:48:14 bde Exp $

SUBDIR=	psd smm usd papers

all-ps:
	@${ECHO} ====\> Making PostScript documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ps ${SUBDIR}

all-ascii:
	@${ECHO} ====\> Making ASCII documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ascii ${SUBDIR}

all:	all-ps all-ascii

install-ps:
	@${ECHO} ====\> Installing PostScript documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ps realinstall

install-ascii:
	@${ECHO} ====\> Installing ASCII documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ascii realinstall

install:	install-ps install-ascii

realinstall:	_SUBDIRUSE

.include <bsd.subdir.mk>
