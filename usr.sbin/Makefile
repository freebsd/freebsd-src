#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $FreeBSD$

# XXX MISSING:		mkproto
SUBDIR=	IPXrouted \
	ac \
	accton \
	adduser \
	amd \
	arp \
	atm \
	bootparamd \
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
	fdcontrol \
	fdformat \
	fdwrite \
	inetd \
	iostat \
	ipfstat \
	ipftest \
	ipmon \
	ipnat \
	ipresend \
	ipsend \
	iptest \
	jail \
	kbdcontrol \
	kbdmap \
	kernbb \
	keyadmin \
	keyserv \
	kvm_mkdb \
	lpr \
	manctl \
	memcontrol \
	mergemaster \
	mixer \
	moused \
	mrouted \
	mtest \
	mtree \
	named \
	named.reload \
	named.restart \
	ndc \
	newsyslog \
	ngctl \
	nghook \
	nslookup \
	nsupdate \
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
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	quotaon \
	rarpd \
	repquota \
	rmt \
	rpc.lockd \
	rpc.statd \
	rpc.umntall \
	rpc.yppasswdd \
	rpc.ypupdated \
	rpc.ypxfrd \
	rtprio \
	rwhod \
	sa \
	sliplogin \
	slstat \
	spray \
	syslogd \
	tcpdchk \
	tcpdmatch \
	tcpdump \
	timed \
	traceroute \
	trpt \
	tzsetup \
	usbd \
	usbdevs \
	vidcontrol \
	vipw \
	vnconfig \
	watch \
	wormcontrol \
	xntpd \
	xten \
	yp_mkdb \
	ypbind \
	yppoll \
	yppush \
	ypserv \
	ypset \
	zic

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
	apmconf \
	apmd \
	boot0cfg \
	btxld \
	i4b \
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
.endif

.include <bsd.subdir.mk>
