#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.98.2.3 1997/06/22 04:29:11 brian Exp $

# XXX MISSING:		mkproto
SUBDIR= ac accton adduser amd arp bootparamd cdcontrol chown chroot \
	cron crunch ctm dev_mkdb diskpart edquota inetd kernbb keyadmin \
	kgmon kvm_mkdb lpr manctl mkdosfs mrouted mtest mtree \
	named named.reload named.restart natd ndc \
	newsyslog nslookup pccard pciconf pcvt pkg_install portmap \
	ppp pppctl pppd pppstats pstat pw pwd_mkdb quot quotaon rarpd \
	repquota rmt rndcontrol rpc.lockd rpc.statd rpc.yppasswdd rpc.ypxfrd \
	rwhod sa sendmail sliplogin slstat \
	spray sup sysctl syslogd tcpdump timed traceroute trpt tzsetup vipw \
	vnconfig watch wormcontrol xntpd xten ypbind yp_mkdb yppoll yppush \
	ypset ypserv zic

SUBDIR+=IPXrouted

.if   make(clean) || make(cleandir)
SUBDIR+=apm apmconf bad144 config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer moused ncrcontrol qcamcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat
.elif ${MACHINE} == "i386"
SUBDIR+=apm apmconf bad144 config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer moused ncrcontrol qcamcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.elif ${MACHINE} == "luna68k"
SUBDIR+=config iostat
.elif ${MACHINE} == "mips"
SUBDIR+=config iostat
.elif ${MACHINE} == "tahoe"
SUBDIR+=config iostat
.endif

.include <bsd.subdir.mk>
