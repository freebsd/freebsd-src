#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.15.2.1 2000/07/11 14:35:27 ps Exp $

SUBDIR= cat \
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
	hostname \
	kill \
	ln \
	ls \
	mkdir \
	mv \
	pax \
	ps \
	pwd \
	rcp \
	rm \
	rmdir \
	sh \
	sleep \
	stty \
	sync \
	test

.if !defined(NO_TCSH)
SUBDIR+=csh
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=rmail
.endif

.include <bsd.subdir.mk>
