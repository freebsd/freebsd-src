#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.147 1999/01/11 18:03:54 msmith Exp $

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
	inetd \
	iostat \
	ipfstat \
	ipftest \
	ipmon \
	ipnat \
	ipresend \
	ipsend \
	iptest \
	kbdcontrol \
	kbdmap \
	kernbb \
	keyadmin \
	keyserv \
	kvm_mkdb \
	lpr \
	manctl \
	moused \
	mrouted \
	mtest \
	mtree \
	named \
	named.reload \
	named.restart \
	ndc \
	newsyslog \
	nslookup \
	nsupdate \
	pccard \
	pciconf \
	periodic \
	pkg_install \
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
	rpc.yppasswdd \
	rpc.ypupdated \
	rpc.ypxfrd \
	rtprio \
	rwhod \
	sa \
	sliplogin \
	slstat \
	spray \
	sysctl \
	syslogd \
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

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=apm \
	apmconf \
	bad144 \
	btxld \
	fdcontrol \
	fdformat \
	fdwrite \
	i4b \
	kgmon \
	lptcontrol \
	mixer \
	mptable \
	pcvt \
	pnpinfo \
	rndcontrol \
	sgsc \
	sicontrol \
	spkrtest \
	stallion \
	wlconfig
.endif

.include <bsd.subdir.mk>
