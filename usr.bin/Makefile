#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

.include <bsd.own.mk>

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX Use GNU versions: diff ld patch
# Moved to secure: bdes
#

SUBDIR=	alias \
	apply \
	asa \
	${_atf} \
	awk \
	banner \
	basename \
	brandelf \
	bsdiff \
	bzip2 \
	bzip2recover \
	cap_mkdb \
	chat \
	chpass \
	cksum \
	${_clang} \
	cmp \
	col \
	colldef \
	colrm \
	column \
	comm \
	compress \
	cpuset \
	csplit \
	ctlstat \
	cut \
	dirname \
	dtc \
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
	find \
	finger \
	fmt \
	fold \
	fstat \
	fsync \
	ftp \
	gcore \
	gencat \
	getconf \
	getent \
	getopt \
	grep \
	gzip \
	head \
	hexdump \
	${_iconv} \
	id \
	ipcrm \
	ipcs \
	join \
	jot \
	${_kdump} \
	keylogin \
	keylogout \
	killall \
	ktrace \
	ktrdump \
	lam \
	lastcomm \
	ldd \
	leave \
	less \
	lessecho \
	lesskey \
	limits \
	locale \
	lock \
	lockf \
	logger \
	login \
	logins \
	logname \
	look \
	lorder \
	lsvfs \
	lzmainfo \
	m4 \
	${_makewhatis} \
	${_man} \
	mandoc \
	mesg \
	minigzip \
	ministat \
	${_mkcsmapper} \
	mkdep \
	${_mkesdb} \
	mkfifo \
	mklocale \
	mktemp \
	mkulzma \
	mkuzip \
	mt \
	ncal \
	netstat \
	newgrp \
	nfsstat \
	nice \
	nl \
	nohup \
	opieinfo \
	opiekey \
	opiepasswd \
	pagesize \
	passwd \
	paste \
	patch \
	pathchk \
	perror \
	pr \
	printenv \
	printf \
	procstat \
	rctl \
	renice \
	rev \
	revoke \
	rpcinfo \
	rs \
	rup \
	rusers \
	rwall \
	script \
	sed \
	seq \
	shar \
	showmount \
	sockstat \
	sort \
	split \
	stat \
	stdbuf \
	su \
	systat \
	tabs \
	tail \
	talk \
	tar \
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
	${_truss} \
	tset \
	tsort \
	tty \
	uname \
	unexpand \
	uniq \
	unzip \
	units \
	unvis \
	uudecode \
	uuencode \
	vi \
	vis \
	vmstat \
	w \
	wall \
	wc \
	what \
	whereis \
	which \
	whois \
	write \
	xargs \
	xinstall \
	${_xlint} \
	${_xstr} \
	xz \
	xzdec \
	${_yacc} \
	yes \
	${_ypcat} \
	${_ypmatch} \
	${_ypwhich}

# NB: keep these sorted by MK_* knobs

.if ${MK_AT} != "no"
SUBDIR+=	at
.endif

.if ${MK_ATF} != "no"
_atf=		atf
.endif

.if ${MK_ATM} != "no"
SUBDIR+=	atm
.endif

.if ${MK_BIND_UTILS} != "no"
SUBDIR+=	dig
SUBDIR+=	host
SUBDIR+=	nslookup
SUBDIR+=	nsupdate
.endif

.if ${MK_BLUETOOTH} != "no"
SUBDIR+=	bluetooth
.endif

.if ${MK_BSD_CPIO} != "no"
SUBDIR+=	cpio
.endif

.if ${MK_CALENDAR} != "no"
SUBDIR+=	calendar
.endif

.if ${MK_CLANG} != "no"
_clang=		clang
.endif

.if ${MK_GROFF} != "no"
SUBDIR+=	vgrind
.endif

.if ${MK_HESIOD} != "no"
SUBDIR+=	hesinfo
.endif

.if ${MK_ICONV} != "no"
_iconv=		iconv
_mkcsmapper=	mkcsmapper
_mkesdb=	mkesdb
.endif

.if ${MK_KDUMP} != "no"
SUBDIR+=        kdump
SUBDIR+=        truss
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
SUBDIR+=	compile_et
.endif

.if ${MK_LIBTHR} != "no"
SUBDIR+=	csup
.endif

.if ${MK_LOCATE} != "no"
SUBDIR+=	locate
.endif

# XXX msgs?
.if ${MK_MAIL} != "no"
SUBDIR+=	biff
SUBDIR+=	from
SUBDIR+=	mail
SUBDIR+=	msgs
.endif

.if ${MK_MAKE} != "no"
.if ${MK_BMAKE} != "no"
SUBDIR+=	bmake
.else
SUBDIR+=	make
.endif
.endif

.if ${MK_MAN_UTILS} != "no"
SUBDIR+=	catman
_makewhatis=	makewhatis
_man=		man
.endif

.if ${MK_NETCAT} != "no"
SUBDIR+=	nc
.endif

.if ${MK_NIS} != "no"
SUBDIR+=	ypcat
SUBDIR+=	ypmatch
SUBDIR+=	ypwhich
.endif

.if ${MK_OPENSSH} != "no"
SUBDIR+=	ssh-copy-id
.endif

.if ${MK_OPENSSL} != "no"
SUBDIR+=	bc
SUBDIR+=	chkey
SUBDIR+=	dc
SUBDIR+=	newkey
.endif

.if ${MK_QUOTAS} != "no"
SUBDIR+=	quota
.endif

.if ${MK_RCMDS} != "no"
SUBDIR+=	rlogin
SUBDIR+=	rsh
SUBDIR+=	ruptime
SUBDIR+=	rwho
.endif

.if ${MK_SENDMAIL} != "no"
SUBDIR+=	vacation
.endif

.if ${MK_TELNET} != "no"
SUBDIR+=	telnet
.endif

.if ${MK_TEXTPROC} != "no"
SUBDIR+=	checknr
SUBDIR+=	colcrt
SUBDIR+=	ul
.endif

.if ${MK_TOOLCHAIN} != "no"
SUBDIR+=	ar
SUBDIR+=	c89
SUBDIR+=	c99
SUBDIR+=	ctags
SUBDIR+=	file2c
SUBDIR+=	gprof
SUBDIR+=	indent
SUBDIR+=	lex
SUBDIR+=	mkstr
SUBDIR+=	rpcgen
SUBDIR+=	unifdef
SUBDIR+=	xlint
SUBDIR+=	xstr
SUBDIR+=	yacc
.endif

.if ${MK_USB} != "no"
SUBDIR+=	usbhidaction
SUBDIR+=	usbhidctl
.endif

.if ${MK_UTMPX} != "no"
SUBDIR+=	last
SUBDIR+=	users
SUBDIR+=	who
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

.include <bsd.subdir.mk>
