#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile,v 1.15.2.2 2000/10/31 09:43:57 dougb Exp $

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

.if !defined(NO_SENDMAIL) && !defined(NOUUCP)
SUBDIR+=rmail
.endif

.include <bsd.subdir.mk>
