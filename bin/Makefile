#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
#	$Id: Makefile,v 1.14 1998/08/30 20:22:15 gpalmer Exp $

SUBDIR= cat \
	chio \
	chmod \
	cp \
	csh \
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

.if !defined(NO_SENDMAIL)
SUBDIR+=rmail
.endif

.include <bsd.subdir.mk>
