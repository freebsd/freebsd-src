#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
#	$Id: Makefile,v 1.109 1998/08/30 20:28:50 gpalmer Exp $

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
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
	chkey \
	cpp \
	ctags \
	cut \
	dirname \
	du \
	ee \
	env \
	error \
	expand \
	f2c \
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
	ftp \
	gencat \
	getopt \
	global \
	head \
	hexdump \
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
	mkdep \
	mkfifo \
	mklocale \
	mkstr \
	mktemp \
	mk_cmds \
	more \
	msgs \
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
	soelim \
	split \
	su \
	symorder \
	tail \
	talk \
	tconv \
	tcopy \
	tee \
	tftp \
	time \
	tip \
	tn3270 \
	top \
	touch \
	tput \
	tr \
	true \
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
SUBDIR+=minigzip

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
# releases do need both
SUBDIR+=telnet
.endif

.if ${MACHINE_ARCH} == "i386"
# Things that don't compile on alpha or are aout specific:
SUBDIR+=ar \
	brandelf \
	dig \
	dnsquery \
	fstat \
	gcore \
	gprof \
	gprof4 \
	host \
	ldd \
	modstat \
	mt \
	nm \
	ranlib \
	sasc \
	size \
	strings \
	strip \
	systat \
	truss \
	vmstat
.if ${OBJFORMAT} == aout
SUBDIR+=doscmd
.endif
.endif

.include <bsd.subdir.mk>
