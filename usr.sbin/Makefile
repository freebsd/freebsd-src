#	@(#)Makefile	5.20 (Berkeley) 6/12/93

# XXX MISSING:		cron mkproto sa
# XXX MISSING SOURCES:	ac quot
# XXX TEMP. BROKEN:	iostat
SUBDIR=	XNSrouted accton amd arp chown chroot dev_mkdb diskpart \
	edquota inetd kgmon kvm_mkdb lpr mtree portmap pstat \
	pwd_mkdb quotaon repquota routed rmt rwhod sendmail \
	sliplogin sysctl syslogd traceroute trpt trsp vipw

.if   make(clean) || make(cleandir)
SUBDIR+=bad144 config config.new eeprom iostat timed update
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat timed update
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 config timed
# iostat
.elif ${MACHINE} == "luna68k"
SUBDIR+=config iostat timed update
.elif ${MACHINE} == "mips"
SUBDIR+=config iostat timed update
.elif ${MACHINE} == "sparc"
SUBDIR+=config.new eeprom timed update
.elif ${MACHINE} == "tahoe"
SUBDIR+=config iostat timed update
.elif ${MACHINE} == "vax"
SUBDIR+=bad144 config iostat timed update
.endif

.include <bsd.subdir.mk>
