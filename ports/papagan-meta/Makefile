PORTNAME=    papagan-meta
PORTVERSION= 0.1
CATEGORIES=  misc
MAINTAINER=  papaganlar07-cyber@example.com
COMMENT=     Papagan meta package that installs default DE, dock and AI assistant

LICENSE=     BSD

RUN_DEPENDS=  plasma5-desktop:desk/plasma5 \
             kwin:de/plasma5-kwin \
             qt5:lang/qt5 \
             x11/qt5 \
             python:lang/python39

PLIST_FILES=  usr/local/bin/papagan-setup

NO_BUILD=    yes

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/bin
	${INSTALL_SCRIPT} ${WRKSRC}/files/papagan-setup ${STAGEDIR}${PREFIX}/bin/papagan-setup
