#	@(#)Makefile	5.20 (Berkeley) 6/12/93

# XXX MISSING:		mkproto
# XXX MISSING SOURCES:	quot
# XXX TEMP. BROKEN:	iostat
SUBDIR= ac accton adduser amd arp bootparamd chown chroot cron crunch ctm \
	dev_mkdb diskpart edquota inetd kernbb kgmon kvm_mkdb lpr lsdev \
	manctl mtree mrouted named nslookup pcvt pkg_install pkg_manage \
	portmap ppp pppd pppstats pstat pwd_mkdb quotaon rarpd repquota \
	routed rmt rwhod sa sendmail sliplogin slstat sysctl syslogd \
	tcpdump timed traceroute trpt tzsetup vipw vnconfig watch xntpd \
	xten ypbind yppoll ypset zic

#
# XNSrouted and trsp are of very marginal utility to anyone at this
# point.  Users who actually have a use for these programs (unlikely)
# are free to add them back in.
#
# SUBDIR+=XNSrouted trsp

.if   make(clean) || make(cleandir)
SUBDIR+=apm apmconf bad144 cdplay config config.new fdformat fdwrite \
	fdcontrol iostat kbdcontrol lptcontrol ncrcontrol rtprio \
	sgsc spkrtest swapinfo vidcontrol 
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat 
.elif ${MACHINE} == "i386"
SUBDIR+=apm apmconf bad144 cdplay config fdformat fdwrite fdcontrol iostat \
	kbdcontrol lptcontrol mixer ncrcontrol rtprio sgsc spkrtest swapinfo \
	vidcontrol
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
