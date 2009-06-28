# Whom:                rodrigc
#
# $FreeBSD$
#

PORTNAME=      jbuild
PORTVERSION=   12
CATEGORIES=    devel

MAINTAINER=    rodrigc@FreeBSD.org
COMMENT=       Utility for building software and tracking build dependencies.

BUILD_DEPENDS += ${LOCALBASE}/lib/mysql/libmysqlclient.a:${PORTSDIR}/databases/mysql60-client

do-fetch:
	${DO_NADA}

do-extract:
	mkdir -p ${WRKSRC}
	( cd ${WRKSRC} ;  ln -s ../../../filemon . ; ln -s ../../../../jbuild .; ln -s ../../../../jdirdep .; ln -s ../../../../make .)
	cp Makefile.port ${WRKSRC}/Makefile

do-install:
	( cd ${WRKSRC}/filemon; ${MAKE} install KMODDIR=/boot/modules)
	( cd ${WRKSRC}/jbuild; ${MAKE} install BINDIR=${PREFIX}/bin )
	( cd ${WRKSRC}/jdirdep; ${MAKE} install BINDIR=${PREFIX}/bin )
	cp ${WRKSRC}/jdirdep/jcommit ${PREFIX}/bin
	cp ${WRKSRC}/jdirdep/jgetsrc ${PREFIX}/bin

do-clean:
	( cd ${WRKSRC}/filemon; ${MAKE} clean )
	( cd ${WRKSRC}/jbuild; ${MAKE} clean )
	( cd ${WRKSRC}/jdirdep; ${MAKE} clean )
	rm -f ${WRKDIR}/\.[a-z]*
	rm -fr ${WRKDIR}

.include <bsd.port.mk>
