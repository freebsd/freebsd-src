# Whom:                rodrigc
#
# $FreeBSD$
#

PORTNAME=      jbuild
PORTVERSION=   10
CATEGORIES=    devel

MAINTAINER=    rodrigc@FreeBSD.org
COMMENT=       Utility for building software and tracking build dependencies.

do-fetch:
	${DO_NADA}

do-extract:
	mkdir -p ${WRKSRC}
	( cd ${WRKSRC} ;  ln -s ../../../* . )
	cp Makefile.port ${WRKSRC}/Makefile

do-install:
	( cd ${WRKSRC}/filemon.ko; ${MAKE} install BINDIR=${PREFIX}/sbin )
	( cd ${WRKSRC}/jbuild; ${MAKE} install BINDIR=${PREFIX}/bin )
	( cd ${WRKSRC}/jdirdep; ${MAKE} install BINDIR=${PREFIX}/bin )
	cp ${WRKSRC}/jdirdep/jgetsrc ${PREFIX}/bin

do-clean:
	( cd ${WRKSRC}/filemon; ${MAKE} clean )
	( cd ${WRKSRC}/jbuild; ${MAKE} clean )
	( cd ${WRKSRC}/jdirdep; ${MAKE} clean )
	rm -f ${WRKDIR}/\.[a-z]*
	rm -fr ${WRKDIR}

.include <bsd.port.mk>
