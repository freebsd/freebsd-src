#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id$

SUBDIR=	psd smm usd papers

all-ps:
	@echo ====\> Making PostScript documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ps ${SUBDIR}

all-ascii:
	@echo ====\> Making ASCII documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ascii ${SUBDIR}

all:	all-ps all-ascii
	
install-ps:
	@echo ====\> Installing PostScript documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ps realinstall

install-ascii:
	@echo ====\> Installing ASCII documents
	@${MAKE} ${MAKEFLAGS} PRINTER=ascii realinstall

install:	install-ps install-ascii

realinstall:	_SUBDIRUSE

.include <bsd.subdir.mk>
