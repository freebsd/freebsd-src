#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch whatis
# Moved to secure: bdes
#

.if ${MACHINE_ARCH} != "ia64"
.if !defined(NO_TOOLCHAIN)
_xlint=	xlint
_lex=	lex
_yacc=	yacc
_c89=	c89
_c99=	c99
.endif
.endif

.if ${MACHINE_ARCH} != "powerpc" && ${MACHINE_ARCH} != "amd64"
.if !defined(NO_TOOLCHAIN)
_gprof=	gprof
.endif
_truss=	truss
.endif

SUBDIR=	alias \
	apply \
	asa \
	at \
	awk \
	banner \
	basename \
	biff \
	brandelf \
	bzip2 \
	bzip2recover \
	${_c89} \
	${_c99} \
	calendar \
	cap_mkdb \
	catman \
	chat \
	checknr \
	chpass \
	cksum \
	cmp \
	col \
	colcrt \
	colldef \
	colrm \
	column \
	comm \
	compile_et \
	compress \
	csplit \
	ctags \
	cut \
	dirname \
	du \
	ee \
	elf2aout \
	elfdump \
	enigma \
	env \
	expand \
	false \
	fetch \
	file \
	file2c \
	find \
	finger \
	fmt \
	fold \
	from \
	fstat \
	fsync \
	ftp \
	gcore \
	gencat \
	getconf \
	getopt \
	${_gprof} \
	head \
	hesinfo \
	hexdump \
	id \
	indent \
	ipcrm \
	ipcs \
	join \
	jot \
	kdump \
	keylogin \
	keylogout \
	killall \
	ktrace \
	ktrdump \
	lam \
	last \
	lastcomm \
	ldd \
	leave \
	less \
	lessecho \
	lesskey \
	${_lex} \
	limits \
	locale \
	locate \
	lock \
	lockf \
	logger \
	login \
	logname \
	look \
	lorder \
	lsvfs \
	m4 \
	mail \
	make \
	makewhatis \
	mesg \
	minigzip \
	mkdep \
	mkfifo \
	mklocale \
	mkstr \
	mktemp \
	msgs \
	mt \
	ncal \
	netstat \
	newgrp \
	nfsstat \
	nice \
	nl \
	nohup \
	objformat \
	opieinfo \
	opiekey \
	opiepasswd \
	pagesize \
	passwd \
	paste \
	pathchk \
	pr \
	printenv \
	printf \
	quota \
	renice \
	rev \
	rlogin \
	rpcgen \
	rpcinfo \
	rs \
	rsh \
	rup \
	ruptime \
	rusers \
	rwall \
	rwho \
	script \
	sed \
	shar \
	showmount \
	sockstat \
	split \
	stat \
	su \
	symorder \
	systat \
	tabs \
	tail \
	talk \
	tcopy \
	tee \
	telnet \
	tftp \
	time \
	tip \
	top \
	touch \
	tput \
	tr \
	true \
	truncate \
	${_truss} \
	tset \
	tsort \
	tty \
	ul \
	uname \
	unexpand \
	unifdef \
	uniq \
	units \
	unvis \
	users \
	uudecode \
	uuencode \
	uuidgen \
	vgrind \
	vi \
	vis \
	vmstat \
	w \
	wall \
	wc \
	what \
	whereis \
	which \
	who \
	whois \
	window \
	write \
	xargs \
	xinstall \
	${_xlint} \
	xstr \
	${_yacc} \
	yes \
	ypcat \
	ypmatch \
	ypwhich

.if !defined(NO_BIND)
SUBDIR+=dig \
	dnskeygen \
	dnsquery \
	host
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=vacation
.endif

.if !defined(NO_USB)
SUBDIR+=usbhidaction \
	usbhidctl
.endif

.if ${MACHINE_ARCH} == "i386"
# Things that don't compile on alpha or are aout specific:
SUBDIR+=doscmd \
	ncplist \
	ncplogin \
	sasc \
	smbutil
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=uac
.endif

.if ${MACHINE_ARCH} == "amd64"
SUBDIR+=ncplist \
	ncplogin \
	smbutil
.endif

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
SUBDIR+=chkey newkey
.endif

.include <bsd.subdir.mk>
