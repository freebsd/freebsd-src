#	@(#)Makefile	5.6.1.2 (Berkeley) 5/8/91

SUBDIR=	accton arp chown chroot config dbsym dev_mkdb diskpart \
	edquota flcopy gettable htable inetd iostat kgmon kvm_mkdb lpr \
	mtree named portmap pppstats pwd_mkdb quotaon repquota rmt rwhod\
	sendmail sliplogin swapinfo syslogd timed traceroute trpt trsp \
	update vipw

.if   make(clean) || make(cleandir)
SUBDIR+=bad144
.elif ${MACHINE} == "i386"
SUBDIR+=bad144 
.endif

.include <bsd.subdir.mk>
