#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX broken:		tconv
# XXX Use GNU versions: apropos bc dc diff grep ld man patch ptx uucp whatis
# Moved to secure: bdes
#
SUBDIR=	alias \
	apply \
	at \
	awk \
	banner \
	basename \
	biff \
	brandelf \
	bzip2 \
	bzip2recover \
	c89 \
	calendar \
	cap_mkdb \
	chat \
	checknr \
	chflags \
	chkey \
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
	ctags \
	cut \
	dirname \
	du \
	ee \
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
	genassym \
	gencat \
	gensetdefs \
	getconf \
	getopt \
	gprof \
	head \
	hexdump \
	id \
	indent \
	ipcrm \
	ipcs \
	join \
	jot \
	kdump \
	kenv \
	key \
	keyinfo \
	keyinit \
	keylogin \
	keylogout \
	killall \
	ktrace \
	kzip \
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
	newkey \
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
	rdist \
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
	soelim \
	split \
	stat \
	su \
	symorder \
	systat \
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
	xlint \
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
SUBDIR+=ar \
	doscmd \
	gcore \
	gprof4 \
	ncplist \
	ncplogin \
	nm \
	ranlib \
	sasc \
	size \
	smbutil \
	strings \
	strip
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=uac
.endif

.include <bsd.subdir.mk>
