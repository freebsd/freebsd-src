#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

SUBDIR= cat \
	chflags \
	chio \
	chmod \
	cp \
	${_csh} \
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
	${_rcp} \
	realpath \
	rm \
	${_rmail} \
	rmdir \
	setfacl \
	sh \
	sleep \
	stty \
	sync \
	test

.if !defined(NO_RCMNDS)
_rcp=	rcp
.endif

.if !defined(NO_SENDMAIL)
_rmail=	rmail
.endif

.if !defined(NO_TCSH)
_csh=	csh
.endif

.include <bsd.subdir.mk>
