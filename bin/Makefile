#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

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
