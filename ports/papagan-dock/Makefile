#
# $FreeBSD$
#

PORTNAME=    papagan-dock
PORTVERSION= 0.1
CATEGORIES=  x11
MAINTAINER=  papaganlar07-cyber@example.com
COMMENT=     Papagan Dock prototype (QtQuick)

LICENSE=     BSD

USES=        qmake qt:5
USE_QT=      qml quickwidgets core gui widgets

PLIST_FILES=  bin/papagan-dock

do-build:
	cd ${WRKSRC} && ${QMAKE} && ${MAKE}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/bin
	${INSTALL_PROGRAM} ${WRKSRC}/papagan-dock ${STAGEDIR}${PREFIX}/bin/papagan-dock
