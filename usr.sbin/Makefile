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
	edquota \
	extattr \
	extattrctl \
	faithd \
	fdcontrol \
	fdformat \
	fdread \
	fdwrite \
	fwcontrol \
	getfmac \
	getpmac \
	ifmcstat \
	inetd \
	iostat \
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
	mountd \
	moused \
	mrouted \
	mtest \
	mtree \
	ndp \
	newsyslog \
	nfsd \
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
	setfmac \
	setkey \
	setpmac \
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
	ugidfw \
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

.if !defined(NO_IPFILTER)
SUBDIR+=ipftest \
	ipresend \
	ipsend \
	iptest
.endif

.if !defined(NOLIBC_R)
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
SUBDIR+=editmap \
	mailstats \
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

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR+=ofwdump
.endif

.if exists(${.CURDIR}/../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
SUBDIR+=keyserv
.endif

.include <bsd.subdir.mk>
