#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.78 1996/01/20 00:58:12 nate Exp $

# XXX MISSING:		mkproto
SUBDIR= ac accton adduser amd arp bootparamd cdcontrol chown chroot \
	cron crunch ctm dev_mkdb diskpart edquota inetd kernbb \
	kgmon kvm_mkdb lpr lsdev manctl mkdosfs mrouted mtree named \
	newsyslog nslookup pcvt pkg_install pkg_manage portmap ppp pppd \
	pppstats pstat pwd_mkdb quot quotaon rarpd repquota rmt \
	rndcontrol routed rwhod sa sendmail sliplogin slstat spray sup sysctl \
	syslogd tcpdump timed traceroute trpt tzsetup vipw vnconfig \
	watch wormcontrol xntpd xten ypbind yppoll yppush ypset ypserv zic

#
# XNSrouted and trsp are of very marginal utility to anyone at this
# point.  Users who actually have a use for these programs (unlikely)
# are free to add them back in.
#
# SUBDIR+=XNSrouted trsp

#
# IPXrouted is part of the optional IPX support, but it doesn't take long
# to build and is _far_ more likely to be used than XNSrouted. :-)
#
SUBDIR+=IPXrouted

.if   make(clean) || make(cleandir)
SUBDIR+=apm apmconf bad144 cdplay config fdformat fdwrite \
	fdcontrol iostat kbdcontrol kbdmap lptcontrol mixer ncrcontrol \
	rtprio sgsc sicontrol spkrtest vidcontrol
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat
.elif ${MACHINE} == "i386"
SUBDIR+=apm apmconf bad144 cdplay config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer ncrcontrol rtprio sgsc sicontrol \
	spkrtest vidcontrol
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
