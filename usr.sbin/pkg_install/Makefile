# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	add create delete info updating version

.include <bsd.subdir.mk>

DATE!=	grep PKG_INSTALL_VERSION ${.CURDIR}/Makefile.inc | sed 's|.*=||'

distfile: clean
	@(cd ${.CURDIR}/..; \
		cp -r pkg_install pkg_install-${DATE}; \
		tar -czf pkg_install/pkg_install-${DATE}.tar.gz \
			--exclude .#* --exclude *~ --exclude CVS \
			--exclude .svn --exclude pkg_install-*.tar.gz \
			pkg_install-${DATE}; \
		rm -rf pkg_install-${DATE})
		
	
