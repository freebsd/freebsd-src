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
	dev_mkdb \
	diskpart \
	edquota \
	faithd \
	fdcontrol \
	fdformat \
	fdwrite \
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
	keyserv \
	lastlogin \
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
	pnpinfo \
	portmap \
	ppp \
	pppctl \
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
	setkey \
	sliplogin \
	slstat \
	spray \
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
	xten \
	yp_mkdb \
	ypbind \
	yppoll \
	yppush \
	ypserv \
	ypset \
	zic

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

.if !defined(NO_MAILWRAPPER)
SUBDIR+=mailwrapper
.endif

.if !defined(NO_SENDMAIL)
SUBDIR+=mailstats \
	makemap \
	praliases \
	sendmail
.endif

.if ${MACHINE_ARCH} == "alpha"
SUBDIR+=elf2exe
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=apm \
	apmd \
	btxld \
	kgmon \
	kgzip \
	lptcontrol \
	mptable \
	pcvt \
	rndcontrol \
	sgsc \
	sicontrol \
	spkrtest \
	stallion \
	wicontrol \
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

.include <bsd.subdir.mk>
