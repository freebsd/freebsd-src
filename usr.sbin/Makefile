#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.134 1998/11/09 23:39:02 dima Exp $

# XXX MISSING:		mkproto
SUBDIR= ac \
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
	crunch \
	ckdist \
	config \
	cron \
	ctm \
	dev_mkdb \
	diskpart \
	edquota \
	inetd \
	iostat \
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
	natd \
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
	rpc.ypxfrd \
	rpc.ypupdated \
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
	vidcontrol \
	vipw \
	vnconfig \
	watch \
	wormcontrol \
	xntpd \
	xten \
	ypbind \
	yp_mkdb \
	yppoll \
	yppush \
	ypset \
	ypserv \
	zic \
	IPXrouted

SUBDIR+=ipfstat \
	ipftest \
	ipmon \
	ipnat \
	ipresend \
	ipsend \
	iptest

.if !defined(NO_SENDMAIL)
SUBDIR+=mailstats \
	makemap \
	praliases \
	sendmail
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=btxld \
	kgmon \
	pcvt \
	pnpinfo \
	rndcontrol \
	wlconfig \

SUBDIR+=apm \
	apmconf \
	bad144 \
	fdformat \
	fdwrite \
	fdcontrol \
	lptcontrol \
	mixer \
	mptable \
	sgsc \
	sicontrol \
	spkrtest \
	stallion
.endif

.include <bsd.subdir.mk>
