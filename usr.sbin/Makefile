#	@(#)Makefile	5.20 (Berkeley) 6/12/93

SUBDIR=	ac accton amd arp chown chroot cron dev_mkdb diskpart edquota \
	inetd kgmon kvm_mkdb lpr mkproto mtree portmap pstat pwd_mkdb \
	quot quotaon repquota rmt rwhod sa sendmail sliplogin sysctl \
	syslogd traceroute trpt trsp update vipw

.if   make(clean) || make(cleandir)
SUBDIR+=bad144 config config.new eeprom iostat timed
.elif ${MACHINE} == "hp300"
SUBDIR+=config iostat timed
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 config iostat
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
