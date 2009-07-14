# Whom:                rodrigc
#
# $FreeBSD$
#

.include "${.CURDIR}/../jbuild_version"

PORTNAME=      jbuild
PORTDIRNAME=   ${PORTNAME}
PORTVERSION=   ${JBUILD_VERSION}
CATEGORIES=    devel

MAINTAINER=    rodrigc@FreeBSD.org
COMMENT=       Utility for building software and tracking build dependencies.

BUILD_DEPENDS += ${LOCALBASE}/lib/mysql/libmysqlclient.a:${PORTSDIR}/databases/mysql60-client

MAN1= jbuild.1

do-fetch:
	${DO_NADA}

do-extract:
	mkdir -p ${WRKSRC}
	( cd ${WRKSRC} ;  ln -s ../../../filemon . ; ln -s ../../../../jbuild .; ln -s ../../../../jdirdep .; ln -s ../../../../make .)
	( cd ${WRKSRC}; cp ../../../../make/make.1 jbuild.1 )
	cp Makefile.port ${WRKSRC}/Makefile

do-install:
	( cd ${WRKSRC}/filemon; ${MAKE} install KMODDIR=/boot/modules)
	( cd ${WRKSRC}/jbuild; ${MAKE} install BINDIR=${PREFIX}/bin )
	( cd ${WRKSRC}/jdirdep; ${MAKE} install BINDIR=${PREFIX}/bin )
	cp ${WRKSRC}/jdirdep/jcommit ${PREFIX}/bin
	cp ${WRKSRC}/jdirdep/jgetsrc ${PREFIX}/bin
	${INSTALL_MAN} ${WRKSRC}/jbuild.1 ${MANPREFIX}/man/man1

do-clean:
	( cd ${WRKSRC}/filemon; ${MAKE} clean )
	( cd ${WRKSRC}/jbuild; ${MAKE} clean )
	( cd ${WRKSRC}/jdirdep; ${MAKE} clean )
	rm -f ${WRKDIR}/\.[a-z]*
	rm -fr ${WRKDIR}

.include <bsd.port.mk>
