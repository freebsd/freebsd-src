#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.117 1998/03/21 13:52:49 peter Exp $

# XXX MISSING:		mkproto
SUBDIR= ac accton adduser amd arp bootparamd cdcontrol chown chroot ckdist \
	cron ctm dev_mkdb diskpart edquota inetd kernbb keyadmin \
	keyserv lpr manctl mkdosfs mrouted mtest mtree \
	named named.reload named.restart ndc newsyslog nslookup \
	pccard pciconf periodic pkg_install portmap \
	ppp pppctl pppd pppstats procctl pw pwd_mkdb quot quotaon rarpd \
	repquota rmt rpc.lockd rpc.statd rpc.yppasswdd rpc.ypxfrd \
	rpc.ypupdated rwhod sa sendmail sliplogin slstat \
	spray sysctl syslogd tcpdump timed traceroute trpt tzsetup vipw \
	vnconfig watch wormcontrol xntpd xten ypbind yp_mkdb \
	yppoll yppush ypset ypserv zic

SUBDIR+=ipfstat ipftest ipmon ipnat ipresend ipsend iptest

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=crunch kgmon kvm_mkdb natd pcvt pnpinfo pstat rndcontrol \
	wlconfig IPXrouted

SUBDIR+=apm apmconf bad144 config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer moused mptable ncrcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.endif

.include <bsd.subdir.mk>
