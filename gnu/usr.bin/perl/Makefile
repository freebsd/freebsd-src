# $FreeBSD$

.if defined(BUILD_SUIDPERL) && ${BUILD_SUIDPERL} == "true"
_suidperl=	suidperl
.endif

SUBDIR= libperl miniperl perl ${_suidperl} library pod utils x2p

MAINTAINER=markm@freebsd.org

build-tools:
	cd ${.CURDIR}/libperl && make build-tools
	cd ${.CURDIR}/miniperl && make build-tools

.include <bsd.subdir.mk>
