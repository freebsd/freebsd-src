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
	cp Makefile.port ${WRKSRC}/Makefile

do-install:
	mkdir -p ${KMODDIR}
	mkdir -p ${PREFIX}/bin
	${MAKE} -C ${WRKSRC}/filemon install KMODDIR=${KMODDIR} \
	    	BINOWN=${BINOWN} BINGRP=${BINGRP}
	${MAKE} -C ${WRKSRC}/jbuild install BINDIR=${PREFIX}/bin \
	    	BINOWN=${BINOWN} BINGRP=${BINGRP}
	${MAKE} -C ${WRKSRC}/jdirdep install BINDIR=${PREFIX}/bin \
	    	BINOWN=${BINOWN} BINGRP=${BINGRP}
	cp ${WRKSRC}/jdirdep/jcommit ${PREFIX}/bin
	cp ${WRKSRC}/jdirdep/jgetsrc ${PREFIX}/bin
	mkdir -p ${MANPREFIX}/man/man1 
	${INSTALL_MAN} ${WRKSRC}/jbuild/jbuild.1 ${MANPREFIX}/man/man1

do-clean:
	( cd ${WRKSRC}/filemon; ${MAKE} clean )
	( cd ${WRKSRC}/jbuild; ${MAKE} clean )
	( cd ${WRKSRC}/jdirdep; ${MAKE} clean )
	rm -f ${WRKDIR}/\.[a-z]*
	rm -fr ${WRKDIR}

.include <bsd.port.mk>
