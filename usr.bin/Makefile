#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
# $FreeBSD$

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX broken:		tconv
# XXX Use GNU versions: apropos bc dc diff grep ld man patch ptx uucp whatis
# Moved to secure: bdes
#
SUBDIR=	apply \
	at \
	banner \
	basename \
	biff \
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
	cpp \
	ctags \
	cut \
	dig \
	dirname \
	dnsquery \
	du \
	ee \
	enigma \
	env \
	error \
	expand \
	false \
	fetch \
	file \
	file2c \
	find \
	finger \
	fmt \
	fold \
	fpr \
	from \
	fsplit \
	fstat \
	ftp \
	gencat \
	gensetdefs \
	getopt \
	global \
	gprof \
	head \
	hexdump \
	host \
	id \
	indent \
	ipcrm \
	ipcs \
	join \
	jot \
	kdump \
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
	mesg \
	minigzip \
	mk_cmds \
	mkdep \
	mkfifo \
	mklocale \
	mkstr \
	mktemp \
	more \
	msgs \
	mt \
	ncal \
	netstat \
	newkey \
	nfsstat \
	nice \
	nohup \
	objformat \
	opieinfo \
	opiekey \
	opiepasswd \
	pagesize \
	passwd \
	paste \
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
	users \
	uudecode \
	uuencode \
	vacation \
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

.if defined(RELEASEDIR) || (!exists(${.CURDIR}/../kerberosIV) && \
    !exists(${.CURDIR}/../secure)) || defined(NOCRYPT) || \
    !defined(MAKE_KERBEROS4)
# Releases need both this non-crypt telnet and the crypt telnet.
SUBDIR+=telnet
.endif

.if ${MACHINE_ARCH} == "i386"
# Things that don't compile on alpha or are aout specific:
SUBDIR+=ar \
	brandelf \
	doscmd \
	gcore \
	gprof4 \
	ncplist \
	ncplogin \
	nm \
	ranlib \
	sasc \
	size \
	strings \
	strip
.endif

.include <bsd.subdir.mk>
