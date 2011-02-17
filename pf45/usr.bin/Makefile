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
	${_grep} \
	gzip \
	head \
	hexdump \
	id \
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
	mesg \
	minigzip \
	ministat \
	mkdep \
	mkfifo \
	mklocale \
	mktemp \
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
	pathchk \
	perror \
	pr \
	printenv \
	printf \
	procstat \
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
	split \
	stat \
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
	truss \
	tset \
	tsort \
	tty \
	uname \
	unexpand \
	uniq \
	unzip \
	units \
	unvis \
	users \
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
	who \
	whois \
	write \
	wtmpcvt \
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

.if ${MK_ATM} != "no"
SUBDIR+=	atm
.endif

.if ${MK_MAN_UTILS} != "no"
SUBDIR+=	catman
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

.if ${MK_BSD_GREP} != "no"
_grep=		grep
.endif

.if ${MK_CALENDAR} != "no"
SUBDIR+=	calendar
.endif

.if ${MK_CLANG} != "no"
_clang=		clang
.endif

.if ${MK_HESIOD} != "no"
SUBDIR+=	hesinfo
.endif

.if ${MK_OPENSSL} != "no"
SUBDIR+=	bc
SUBDIR+=	chkey
SUBDIR+=	dc
SUBDIR+=	newkey
.if ${MK_LIBTHR} != "no"
SUBDIR+=	csup
.endif
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
SUBDIR+=	make
.endif

.if ${MK_MAN_UTILS} != "no"
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
SUBDIR+=	compile_et
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
# XXX maybe under textproc?
SUBDIR+=	vgrind
SUBDIR+=	yacc
.endif

.if ${MK_USB} != "no"
SUBDIR+=	usbhidaction
SUBDIR+=	usbhidctl
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

.include <bsd.subdir.mk>
