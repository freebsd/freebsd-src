#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.23 2003/02/18 19:51:58 obrien Exp $

SUBDIR= cat \
	chflags \
	chio \
	chmod \
	cp \
	date \
	dd \
	df \
	domainname \
	echo \
	ed \
	expr \
	getfacl \
	hostname \
	kenv \
	kill \
	ln \
	ls \
	mkdir \
	mv \
	pax \
	ps \
	pwd \
	realpath \
	rm \
	rmdir \
	setfacl \
	sh \
	sleep \
	stty \
	sync \
	test

.if !defined(NO_RCMNDS)
SUBDIR+=rcp
.endif

.if !defined(NO_TCSH)
SUBDIR+=csh
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=rmail
.endif

.include <bsd.subdir.mk>
