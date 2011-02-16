# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.25.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	lib add create delete info updating version

.include <bsd.subdir.mk>

DATE!=	grep PKG_INSTALL_VERSION ${.CURDIR}/lib/lib.h | sed 's|.*[ 	]||'

distfile: clean
	@(cd ${.CURDIR}/..; \
		cp -r pkg_install pkg_install-${DATE}; \
		tar -czf pkg_install/pkg_install-${DATE}.tar.gz \
			--exclude .#* --exclude *~ --exclude CVS \
			--exclude .svn --exclude pkg_install-*.tar.gz \
			pkg_install-${DATE}; \
		rm -rf pkg_install-${DATE})
		
	
