# $FreeBSD: src/gnu/usr.bin/perl/Makefile,v 1.9.2.1 2000/10/05 09:54:37 markm Exp $

SUBDIR= libperl miniperl perl suidperl library pod utils x2p

MAINTAINER=markm@freebsd.org

build-tools:
	cd ${.CURDIR}/libperl && make build-tools
	cd ${.CURDIR}/miniperl && make build-tools

.include <bsd.subdir.mk>
