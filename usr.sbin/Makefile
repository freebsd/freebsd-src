#	@(#)Makefile	5.20 (Berkeley) 6/12/93

# XXX MISSING:		cron mkproto sa
# XXX MISSING SOURCES:	ac quot
# XXX TEMP. BROKEN:	iostat xntpd
SUBDIR=	accton amd arp chown chroot cron dbsym dev_mkdb diskpart \
	edquota inetd kbdcontrol kgmon kvm_mkdb lpr mtree portmap pstat \
	pwd_mkdb quotaon repquota routed rmt rtprio rwhod sendmail \
	sliplogin sysctl syslogd tcpdump traceroute trpt vidcontrol \
	vipw ypbind yppoll ypset

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
