# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	lib add create delete info updating version

.include <bsd.subdir.mk>

CP=	/bin/cp
RM=	/bin/rm
TAR=	/usr/bin/tar

DATE!=	date +%Y%m%d

distfile: clean
	@(cd ${.CURDIR}/..; \
		${CP} -r pkg_install pkg_install-${DATE}; \
		${TAR} -czf pkg_install/pkg_install-${DATE}.tar.gz \
			--exclude .#* --exclude *~ --exclude CVS \
			--exclude pkg_install-*.tar.gz pkg_install-${DATE}; \
		${RM} -rf pkg_install-${DATE})
		
	
