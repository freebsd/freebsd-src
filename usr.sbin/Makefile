#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
#	$Id: Makefile,v 1.62.4.8 1996/06/25 20:39:37 markm Exp $

# XXX MISSING:		mkproto
SUBDIR= ac accton adduser amd arp bootparamd cdcontrol chown chroot cron \
	crunch ctm dev_mkdb diskpart edquota inetd kernbb kgmon \
	kvm_mkdb lpr lsdev manctl mrouted mtree named nslookup \
	pcvt pkg_install portmap ppp pppd pppstats \
	pstat pwd_mkdb quot quotaon rarpd repquota rndcontrol \
	routed rmt rwhod sa sendmail sliplogin slstat sup sysctl \
	syslogd tcpdump timed traceroute trpt tzsetup vipw vnconfig \
	watch wormcontrol xntpd xten ypbind yppoll ypset zic

#
# XNSrouted and trsp are of very marginal utility to anyone at this
# point.  Users who actually have a use for these programs (unlikely)
# are free to add them back in.
#
# SUBDIR+=XNSrouted trsp

.if   make(clean) || make(cleandir)
SUBDIR+=apm apmconf bad144 cdplay config fdformat fdwrite \
	fdcontrol iostat kbdcontrol kbdmap lptcontrol mixer ncrcontrol \
	rtprio sgsc sicontrol spkrtest stallion vidcontrol
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat
.elif ${MACHINE} == "i386"
SUBDIR+=apm apmconf bad144 cdplay config fdformat fdwrite fdcontrol iostat \
	kbdcontrol kbdmap lptcontrol mixer ncrcontrol rtprio sgsc sicontrol \
	spkrtest stallion vidcontrol
.elif ${MACHINE} == "luna68k"
SUBDIR+=config iostat
.elif ${MACHINE} == "mips"
SUBDIR+=config iostat
.elif ${MACHINE} == "sparc"
SUBDIR+=config.new eeprom
.elif ${MACHINE} == "tahoe"
SUBDIR+=config iostat
.elif ${MACHINE} == "vax"
SUBDIR+=bad144 config iostat
.endif

.include <bsd.subdir.mk>
