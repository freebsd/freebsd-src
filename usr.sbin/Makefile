#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.124 1998/08/17 05:48:22 gpalmer Exp $

# XXX MISSING:		mkproto
SUBDIR= ac accton adduser amd arp bootparamd cdcontrol chkgrp chown chroot \
	ckdist config cron ctm dev_mkdb diskpart edquota inetd kernbb \
	keyadmin keyserv kvm_mkdb lpr manctl mrouted mtest mtree \
	named named.reload named.restart natd ndc newsyslog nslookup nsupdate \
	pccard pciconf periodic pkg_install portmap \
	ppp pppctl pppd pppstats procctl pw pwd_mkdb quot quotaon rarpd \
	repquota rmt rpc.lockd rpc.statd rpc.yppasswdd rpc.ypxfrd \
	rpc.ypupdated rwhod sa sliplogin slstat \
	spray sysctl syslogd tcpdump timed traceroute trpt tzsetup vipw \
	vnconfig watch wormcontrol xntpd xten ypbind yp_mkdb \
	yppoll yppush ypset ypserv zic

SUBDIR+=ipfstat ipftest ipmon ipnat ipresend ipsend iptest

.if !defined(NO_SENDMAIL)
SUBDIR+=mailstats makemap praliases sendmail
.endif

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=crunch kgmon pcvt pnpinfo pstat rndcontrol \
	wlconfig IPXrouted

SUBDIR+=apm apmconf bad144 fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer moused mptable ncrcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.endif

.include <bsd.subdir.mk>
