#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

.include <bsd.own.mk>

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch whatis
# Moved to secure: bdes
#

SUBDIR=	alias \
	apply \
	${_ar} \
	asa \
	${_at} \
	${_atm} \
	awk \
	banner \
	basename \
	${_biff} \
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
	${_checknr} \
	${_chkey} \
	chpass \
	cksum \
	cmp \
	col \
	${_colcrt} \
	colldef \
	colrm \
	column \
	comm \
	${_compile_et} \
	compress \
	${_cpio} \
	cpuset \
	csplit \
	${_csup} \
	${_ctags} \
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
	${_file2c} \
	find \
	finger \
	${_fmt} \
	fold \
	${_from} \
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
	${_indent} \
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
	${_locate} \
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
	${_mail} \
	${_make} \
	makewhatis \
	mesg \
	minigzip \
	ministat \
	mkdep \
	mkfifo \
	mklocale \
	${_mkstr} \
	mktemp \
	mkuzip \
	${_msgs} \
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
	perror \
	pr \
	printenv \
	printf \
	procstat \
	${_quota} \
	renice \
	rev \
	revoke \
	${_rlogin} \
	${_rpcgen} \
	rpcinfo \
	rs \
	${_rsh} \
	rup \
	${_ruptime} \
	rusers \
	rwall \
	${_rwho} \
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
	${_telnet} \
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
	${_ul} \
	uname \
	unexpand \
	${_unifdef} \
	uniq \
	unzip \
	units \
	unvis \
	${_usbhidaction} \
	${_usbhidctl} \
	users \
	uudecode \
	uuencode \
	${_vacation} \
	${_vgrind} \
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
	${_yacc} \
	yes \
	${_ypcat} \
	${_ypmatch} \
	${_ypwhich}

.if ${MACHINE_ARCH} != "arm"
_truss=		truss
.endif

# NB: keep these sorted by MK_* knobs

.if ${MK_AT} != "no"
_at=		at
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

.if ${MK_BSD_CPIO} != "no"
_cpio=		cpio
.endif

.if ${MK_CALENDAR} != "no"
_calendar=	calendar
.endif

.if ${MK_HESIOD} != "no"
_hesinfo=	hesinfo
.endif

.if ${MK_OPENSSL} != "no"
_chkey=		chkey
_newkey=	newkey
.if ${MK_LIBTHR} != "no"
_csup=		csup
.endif
.endif

.if ${MK_LOCATE} != "no"
_locate=	locate
.endif

# XXX msgs?
.if ${MK_MAIL} != "no"
_biff=		biff
_fmt=		fmt
_from=		from
_mail=		mail
_msgs=		msgs
.endif

.if ${MK_MAKE} != "no"
_make=		make
.endif

.if ${MK_NETCAT} != "no"
_nc=		nc
.endif

.if ${MK_NIS} != "no"
_ypcat=		ypcat
_ypmatch=	ypmatch
_ypwhich=	ypwhich
.endif

.if ${MK_QUOTAS} != "no"
_quota=		quota
.endif

.if ${MK_RCMDS} != "no"
_rlogin=	rlogin
_rsh=		rsh
_ruptime=	ruptime
_rwho=		rwho
.endif

.if ${MK_SENDMAIL} != "no"
_vacation=	vacation
.endif

.if ${MK_TELNET} != "no"
_telnet=	telnet
.endif

.if ${MK_TEXTPROC} != "no"
_checknr=	checknr
_colcrt=	colcrt
_ul=		ul
.endif

.if ${MK_TOOLCHAIN} != "no"
_ar=		ar
_c89=		c89
_c99=		c99
_compile_et=	compile_et
_ctags=		ctags
_file2c=	file2c
_gprof=		gprof
_indent=	indent
_lex=		lex
_mkstr=		mkstr
_rpcgen=	rpcgen
_unifdef=	unifdef
_xlint=		xlint
_xstr=		xstr
# XXX maybe under textproc?
_vgrind=	vgrind
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
