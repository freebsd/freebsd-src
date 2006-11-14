#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch whatis
# Moved to secure: bdes
#

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
	calendar \
	cap_mkdb \
	catman \
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
	head \
	hesinfo \
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
	objformat \
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
	${_uac} \
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

.if ${MACHINE_ARCH} != "arm" && \
    ${MACHINE_ARCH} != "powerpc"
_truss=		truss
.endif

.if !defined(NO_ATM)
_atm=		atm
.endif

.if !defined(NO_BIND) && !defined(NO_BIND_UTILS)
_dig=		dig
_host=		host
_nslookup=	nslookup
_nsupdate=	nsupdate
.endif

.if !defined(NO_BLUETOOTH)
_bluetooth=	bluetooth
.endif

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
_chkey=		chkey
_newkey=	newkey
.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "sparc64"
.if !defined(NO_LIBTHR)
_csup=		csup
.endif
.elif !defined(NO_LIBPTHREAD)
_csup=		csup
.endif
.endif

.if !defined(NO_NETCAT)
_nc=		nc
.endif

.if !defined(NO_NIS) 
_ypcat=		ypcat
_ypmatch=	ypmatch
_ypwhich=	ypwhich
.endif

.if !defined(NO_RCMDS)
_rlogin=	rlogin
_rsh=		rsh
.endif

.if !defined(NO_SENDMAIL)
_vacation=	vacation
.endif

.if !defined(NO_TOOLCHAIN)
_c89=		c89
_c99=		c99
.if ${MACHINE_ARCH} != "powerpc"
_gprof=		gprof
.endif
_lex=		lex
_xlint=		xlint
_yacc=		yacc
.endif

.if !defined(NO_USB)
_usbhidaction=	usbhidaction
_usbhidctl=	usbhidctl
.endif

.if ${MACHINE_ARCH} == "i386"
_ncplist=	ncplist
_ncplogin=	ncplogin
_smbutil=	smbutil
.endif

.if ${MACHINE_ARCH} == "alpha"
_uac=		uac
.endif

.if ${MACHINE_ARCH} == "amd64"
_ncplist=	ncplist
_ncplogin=	ncplogin
_smbutil=	smbutil
.endif

.include <bsd.subdir.mk>
