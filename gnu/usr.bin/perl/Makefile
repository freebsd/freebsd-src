# $FreeBSD$

SUBDIR= libperl miniperl perl suidperl library pod utils x2p

MAINTAINER=markm@freebsd.org

build-tools:
	cd ${.CURDIR}/libperl && make build-tools
	cd ${.CURDIR}/miniperl && make build-tools

.include <bsd.subdir.mk>
