#	@(#)Makefile	5.20 (Berkeley) 6/12/93

# XXX MISSING:		mkproto
# XXX MISSING SOURCES:	quot
# XXX TEMP. BROKEN:	iostat
SUBDIR=	ac accton amd arp chown chroot \
	cron crunch dbsym dev_mkdb diskpart edquota fdformat fdwrite \
	inetd kbdcontrol kgmon kvm_mkdb lpr lptcontrol \
	manctl mtree mrouted named nslookup pkg_install portmap pstat \
	pwd_mkdb quotaon repquota routed rmt rtprio rwhod sa sendmail \
	sliplogin sysctl syslogd tcpdump traceroute trpt tzsetup vidcontrol \
	vipw xntpd ypbind yppoll ypset zic

#
# XNSrouted and trsp are of very marginal utility to anyone at this
# point.  Users who actually have a use for these programs (unlikely)
# are free to add them back in.
#
# SUBDIR+=XNSrouted trsp

.if   make(clean) || make(cleandir)
SUBDIR+=bad144 config config.new eeprom iostat swapinfo timed 
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat timed 
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 config swapinfo timed
# iostat
.elif ${MACHINE} == "luna68k"
SUBDIR+=config iostat timed 
.elif ${MACHINE} == "mips"
SUBDIR+=config iostat timed 
.elif ${MACHINE} == "sparc"
SUBDIR+=config.new eeprom timed 
.elif ${MACHINE} == "tahoe"
SUBDIR+=config iostat timed 
.elif ${MACHINE} == "vax"
SUBDIR+=bad144 config iostat timed 
.endif

.include <bsd.subdir.mk>
