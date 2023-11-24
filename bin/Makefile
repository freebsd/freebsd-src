#	From: @(#)Makefile	8.1 (Berkeley) 5/31/93

.include <src.opts.mk>

SUBDIR= cat \
	chflags \
	chio \
	chmod \
	cp \
	cpuset \
	date \
	dd \
	df \
	domainname \
	echo \
	ed \
	expr \
	freebsd-version \
	getfacl \
	hostname \
	kenv \
	kill \
	ln \
	ls \
	mkdir \
	mv \
	nproc \
	pax \
	pkill \
	ps \
	pwait \
	pwd \
	realpath \
	rm \
	rmdir \
	setfacl \
	sh \
	sleep \
	stty \
	sync \
	test \
	timeout \
	uuidgen

SUBDIR.${MK_SENDMAIL}+=	rmail
SUBDIR.${MK_TCSH}+=	csh
SUBDIR.${MK_TESTS}+=	tests

.include <bsd.arch.inc.mk>

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
