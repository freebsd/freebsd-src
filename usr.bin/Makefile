#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX broken:		tconv
# XXX Use GNU versions: apropos bc dc diff grep ld man patch whatis
# Moved to secure: bdes
#

.if ${MACHINE_ARCH} != "ia64"
_xlint=	xlint
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
	c89 \
	c99 \
	calendar \
	cap_mkdb \
	catman \
	chat \
	checknr \
	chflags \
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
	gprof \
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
	kenv \
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
	lex \
	limits \
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
	tftp \
	time \
	tip \
	top \
	touch \
	tput \
	tr \
	true \
	truncate \
	truss \
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
	usbhidctl \
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
	yacc \
	yes \
	ypcat \
	ypmatch \
	ypwhich

.if defined(RELEASEDIR) || \
    (!exists(${.CURDIR}/../kerberosIV) && !exists(${.CURDIR}/../secure)) || \
    defined(NOCRYPT) || (defined(NOSECURE) && !defined(MAKE_KERBEROS4))
# Releases need both this non-crypt telnet and the crypt telnet.
SUBDIR+=telnet
.endif

.if !defined(NO_BIND)
SUBDIR+=dig \
	dnskeygen \
	dnsquery \
	host
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=vacation
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

.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
SUBDIR+=chkey newkey
.endif

.include <bsd.subdir.mk>
