#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

.include <bsd.own.mk>

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch whatis
# Moved to secure: bdes
#

.include <bsd.own.mk>

SUBDIR=	alias \
	apply \
	asa \
	at \
	${_atm} \
	awk \
	banner \
	basename \
	biff \
	${_bluetooth} \
	brandelf \
	bsdiff \
	bzip2 \
	bzip2recover \
	${_c89} \
	${_c99} \
	${_calendar} \
	cap_mkdb \
	${_catman} \
	chat \
	checknr \
	${_chkey} \
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
	${_csup} \
	ctags \
	cut \
	${_dig} \
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
	getent \
	getopt \
	${_gprof} \
	gzip \
	head \
	${_hesinfo} \
	hexdump \
	${_host} \
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
	logins \
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
	mkuzip \
	msgs \
	mt \
	${_nc} \
	ncal \
	${_ncplist} \
	${_ncplogin} \
	netstat \
	newgrp \
	${_newkey} \
	nfsstat \
	nice \
	nl \
	nohup \
	${_nslookup} \
	${_nsupdate} \
	opieinfo \
	opiekey \
	opiepasswd \
	pagesize \
	passwd \
	paste \
	pathchk \
	pkill \
	pr \
	printenv \
	printf \
	quota \
	renice \
	rev \
	${_rlogin} \
	rpcgen \
	rpcinfo \
	rs \
	${_rsh} \
	rup \
	ruptime \
	rusers \
	rwall \
	rwho \
	script \
	sed \
	shar \
	showmount \
	${_smbutil} \
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
	${_usbhidaction} \
	${_usbhidctl} \
	users \
	uudecode \
	uuencode \
	uuidgen \
	${_vacation} \
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
	${_ypcat} \
	${_ypmatch} \
	${_ypwhich}

.if ${MACHINE_ARCH} != "arm"
_truss=		truss
.endif

.if ${MK_ATM} != "no"
_atm=		atm
.endif

.if ${MK_MAN} != "no"
_catman=	catman
.endif

.if ${MK_BIND_UTILS} != "no"
_dig=		dig
_host=		host
_nslookup=	nslookup
_nsupdate=	nsupdate
.endif

.if ${MK_BLUETOOTH} != "no"
_bluetooth=	bluetooth
.endif

.if ${MK_CALENDAR} != "no"
_calendar=	calendar
.endif

.if ${MK_OPENSSL} != "no"
_chkey=		chkey
_newkey=	newkey
.if ${MK_LIBTHR} != "no" || \
    (${MACHINE_ARCH} != "sparc64" && ${MK_LIBPTHREAD} != "no")   
_csup=		csup
.endif
.endif

.if ${MK_HESIOD} != "no"
_hesinfo=	hesinfo
.endif

.if ${MK_NETCAT} != "no"
_nc=		nc
.endif

.if ${MK_NIS} != "no"
_ypcat=		ypcat
_ypmatch=	ypmatch
_ypwhich=	ypwhich
.endif

.if ${MK_RCMDS} != "no"
_rlogin=	rlogin
_rsh=		rsh
.endif

.if ${MK_SENDMAIL} != "no"
_vacation=	vacation
.endif

.if ${MK_TOOLCHAIN} != "no"
_c89=		c89
_c99=		c99
_gprof=		gprof
_lex=		lex
_xlint=		xlint
_yacc=		yacc
.endif

.if ${MK_USB} != "no"
_usbhidaction=	usbhidaction
_usbhidctl=	usbhidctl
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MK_NCP} != "no"
_ncplist=	ncplist
_ncplogin=	ncplogin
.endif
_smbutil=	smbutil
.endif

.if ${MACHINE_ARCH} == "ia64"
_smbutil=	smbutil
.endif

.if ${MACHINE_ARCH} == "amd64"
.if ${MK_NCP} != "no"
_ncplist=	ncplist
_ncplogin=	ncplogin
.endif
_smbutil=	smbutil
.endif

.if ${MACHINE_ARCH} == "powerpc"
_smbutil=	smbutil
.endif

.include <bsd.subdir.mk>
