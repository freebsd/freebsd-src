#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.85 1996/04/04 03:17:48 peter Exp $

# XXX MISSING:		mkproto
SUBDIR= ac accton adduser amd arp bootparamd cdcontrol chown chroot \
	cron crunch ctm dev_mkdb diskpart edquota inetd kernbb \
	kgmon kvm_mkdb lpr lsdev manctl mkdosfs mrouted mtree named \
	newsyslog nslookup pcvt pkg_install portmap ppp pppd \
	pppstats pstat pwd_mkdb quot quotaon rarpd repquota rmt \
	rndcontrol routed rpc.lockd rpc.statd rpc.yppasswdd rwhod sa \
	sendmail sliplogin slstat \
	spray sup sysctl syslogd tcpdump timed traceroute trpt tzsetup vipw \
	vnconfig watch wormcontrol xntpd xten ypbind yppoll yppush ypset \
	ypserv zic

SUBDIR+=IPXrouted

.if   make(clean) || make(cleandir)
SUBDIR+=apm apmconf bad144 config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer ncrcontrol qcamcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat
.elif ${MACHINE} == "i386"
SUBDIR+=apm apmconf bad144 config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer ncrcontrol qcamcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.elif ${MACHINE} == "luna68k"
SUBDIR+=config iostat
.elif ${MACHINE} == "mips"
SUBDIR+=config iostat
.elif ${MACHINE} == "sparc"
SUBDIR+=eeprom
.elif ${MACHINE} == "tahoe"
SUBDIR+=config iostat
.endif

.include <bsd.subdir.mk>
