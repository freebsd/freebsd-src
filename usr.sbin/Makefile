#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $FreeBSD$

# XXX MISSING:		mkproto
SUBDIR=	IPXrouted \
	ac \
	accton \
	adduser \
	amd \
	ancontrol \
	arp \
	atm \
	bootparamd \
	burncd \
	cdcontrol \
	chkgrp \
	chown \
	chroot \
	ckdist \
	config \
	cron \
	crunch \
	ctm \
	daemon \
	dev_mkdb \
	devinfo \
	digictl \
	diskpart \
	edquota \
	extattrctl \
	faithd \
	fdcontrol \
	fdformat \
	fdread \
	fdwrite \
	getextattr \
	gifconfig \
	ifmcstat \
	inetd \
	iostat \
	ipftest \
	ipresend \
	ipsend \
	iptest \
	jail \
	kbdcontrol \
	kbdmap \
	kernbb \
	kldxref \
	lastlogin \
	mailwrapper \
	manctl \
	memcontrol \
	mergemaster \
	mixer \
	mld6query \
	mlxcontrol \
	moused \
	mrouted \
	mtest \
	mtree \
	ndp \
	newsyslog \
	ngctl \
	ntp \
	nghook \
	pccard \
	pciconf \
	periodic \
	pkg_install \
	ppp \
	pppd \
	pppstats \
	prefix \
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	quotaon \
	rarpd \
	raycontrol \
	repquota \
	rip6query \
	rmt \
	route6d \
	rpcbind \
	rpc.lockd \
	rpc.statd \
	rpc.umntall \
	rpc.yppasswdd \
	rpc.ypupdated \
	rpc.ypxfrd \
	rrenumd \
	rtadvd \
	rtprio \
	rtsold \
	rwhod \
	sa \
	setextattr \
	setkey \
	sliplogin \
	slstat \
	spray \
	sysinstall \
	syslogd \
	tcpdchk \
	tcpdmatch \
	tcpdump \
	timed \
	traceroute \
	traceroute6 \
	trpt \
	tzsetup \
	usbd \
	usbdevs \
	vidcontrol \
	vipw \
	vnconfig \
	watch \
	wicontrol \
	xten \
	yp_mkdb \
	ypbind \
	yppoll \
	yppush \
	ypserv \
	ypset \
	zic

.if !defined(NOLIBC_R) && ${MACHINE_ARCH} != "ia64"
SUBDIR+=pppctl
.endif

.if !defined(NO_BIND)
SUBDIR+=named \
	named.reload \
	named.restart \
	ndc \
	nslookup \
	nsupdate
.endif

.if !defined(NO_LPR)
SUBDIR+=lpr
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=mailstats \
	makemap \
	praliases \
	sendmail
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=elf2exe \
	pnpinfo
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=acpi \
	apm \
	apmd \
	btxld \
	kgmon \
	kgzip \
	lptcontrol \
	mptable \
	pcvt \
	pnpinfo \
	sgsc \
	sicontrol \
	spkrtest \
	stallion \
	wlconfig
.if !defined(NO_I4B)
SUBDIR+=i4b
.endif
.endif

.if ${MACHINE} == "i386"
SUBDIR+=boot0cfg
.endif

.if ${MACHINE} == "pc98"
SUBDIR+=boot98cfg
.endif

.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
SUBDIR+=keyserv
.endif

.include <bsd.subdir.mk>
