#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	adduser \
	arp \
	binmiscctl \
	bsdconfig \
	camdd \
	cdcontrol \
	chkgrp \
	chown \
	chroot \
	ckdist \
	clear_locks \
	crashinfo \
	cron \
	ctladm \
	ctld \
	daemon \
	dconschat \
	devctl \
	devinfo \
	digictl \
	diskinfo \
	dumpcis \
	extattr \
	extattrctl \
	fifolog \
	fstyp \
	fwcontrol \
	getfmac \
	getpmac \
	gstat \
	i2c \
	ifmcstat \
	iostat \
	kldxref \
	mailwrapper \
	makefs \
	memcontrol \
	mergemaster \
	mfiutil \
	mixer \
	mlxcontrol \
	mountd \
	mount_smbfs \
	mptutil \
	mtest \
	${_mtree} \
	newsyslog \
	nfscbd \
	nfsd \
	nfsdumpstate \
	nfsrevoke \
	nfsuserd \
	nmtree \
	nologin \
	${_pc_sysinstall} \
	pciconf \
	periodic \
	powerd \
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	rarpd \
	rmt \
	rpcbind \
	rpc.lockd \
	rpc.statd \
	rpc.umntall \
	rtprio \
	service \
	services_mkdb \
	sesutil \
	setfib \
	setfmac \
	setpmac \
	smbmsg \
	snapinfo \
	spray \
	syslogd \
	sysrc \
	tcpdrop \
	tcpdump \
	traceroute \
	trpt \
	tzsetup \
	uefisign \
	ugidfw \
	vigr \
	vipw \
	wake \
	watch \
	watchdogd \
	zic

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
SUBDIR+=	accton
SUBDIR+=	sa
.endif

.if ${MK_AMD} != "no"
SUBDIR+=	amd
.endif

.if ${MK_AUDIT} != "no"
SUBDIR+=	audit
SUBDIR+=	auditd
.if ${MK_OPENSSL} != "no"
SUBDIR+=	auditdistd
.endif
SUBDIR+=	auditreduce
SUBDIR+=	praudit
.endif

.if ${MK_AUTHPF} != "no"
SUBDIR+=	authpf
.endif

.if ${MK_AUTOFS} != "no"
SUBDIR+=	autofs
.endif

.if ${MK_BLUETOOTH} != "no"
SUBDIR+=	bluetooth
.endif

.if ${MK_BOOTPARAMD} != "no"
SUBDIR+=	bootparamd
.endif

.if ${MK_BSDINSTALL} != "no"
SUBDIR+=	bsdinstall
.endif

.if ${MK_BSNMP} != "no"
SUBDIR+=	bsnmpd
.endif

.if ${MK_CTM} != "no"
SUBDIR+=	ctm
.endif

.if ${MK_FLOPPY} != "no"
SUBDIR+=	fdcontrol
SUBDIR+=	fdformat
SUBDIR+=	fdread
SUBDIR+=	fdwrite
.endif

.if ${MK_FMTREE} != "no"
SUBDIR+=	mtree
.endif

.if ${MK_FREEBSD_UPDATE} != "no"
SUBDIR+=	freebsd-update
.endif

.if ${MK_GSSAPI} != "no"
SUBDIR+=	gssd
.endif

.if ${MK_GPIO} != "no"
SUBDIR+=	gpioctl
.endif

.if ${MK_INET6} != "no"
SUBDIR+=	faithd
SUBDIR+=	ip6addrctl
SUBDIR+=	mld6query
SUBDIR+=	ndp
SUBDIR+=	rip6query
SUBDIR+=	route6d
SUBDIR+=	rrenumd
SUBDIR+=	rtadvctl
SUBDIR+=	rtadvd
SUBDIR+=	rtsold
SUBDIR+=	traceroute6
.endif

.if ${MK_INETD} != "no"
SUBDIR+=	inetd
.endif

.if ${MK_IPFW} != "no"
SUBDIR+=	ipfwpcap
.endif

.if ${MK_IPX} != "no"
SUBDIR+=	IPXrouted
.endif

.if ${MK_ISCSI} != "no"
SUBDIR+=	iscsid
.endif

.if ${MK_JAIL} != "no"
SUBDIR+=	jail
SUBDIR+=	jexec
SUBDIR+=	jls
.endif

# XXX MK_SYSCONS
.if ${MK_LEGACY_CONSOLE} != "no"
SUBDIR+=	kbdcontrol
SUBDIR+=	kbdmap
SUBDIR+=	moused
SUBDIR+=	vidcontrol
.endif

.if ${MK_LIBTHR} != "no" || ${MK_LIBPTHREAD} != "no"
.if ${MK_PPP} != "no"
SUBDIR+=	pppctl
.endif
.if ${MK_NS_CACHING} != "no"
SUBDIR+=	nscd
.endif
.endif

.if ${MK_LPR} != "no"
SUBDIR+=	lpr
.endif

.if ${MK_MAN_UTILS} != "no"
SUBDIR+=	manctl
.endif

.if ${MK_NAND} != "no"
SUBDIR+=	nandsim
SUBDIR+=	nandtool
.endif

.if ${MK_NETGRAPH} != "no"
SUBDIR+=	flowctl
SUBDIR+=	lmcconfig
SUBDIR+=	ngctl
SUBDIR+=	nghook
.endif

.if ${MK_NIS} != "no"
SUBDIR+=	rpc.yppasswdd
SUBDIR+=	rpc.ypupdated
SUBDIR+=	rpc.ypxfrd
SUBDIR+=	ypbind
SUBDIR+=	yp_mkdb
SUBDIR+=	yppoll
SUBDIR+=	yppush
SUBDIR+=	ypserv
SUBDIR+=	ypset
.endif

.if ${MK_NTP} != "no"
SUBDIR+=	ntp
.endif

.if ${MK_OPENSSL} != "no"
SUBDIR+=	keyserv
.endif

.if ${MK_PC_SYSINSTALL} != "no"
_pc_sysinstall=	pc-sysinstall
.endif

.if ${MK_PF} != "no"
SUBDIR+=	ftp-proxy
.endif

.if ${MK_PKGBOOTSTRAP} != "no"
SUBDIR+=	pkg
.endif

.if ${MK_PKGTOOLS} != "no"
SUBDIR+=	pkg_install
.endif

# XXX MK_TOOLCHAIN?
.if ${MK_PMC} != "no"
SUBDIR+=	pmcannotate
SUBDIR+=	pmccontrol
SUBDIR+=	pmcstat
SUBDIR+=	pmcstudy
.endif

.if ${MK_PORTSNAP} != "no"
SUBDIR+=	portsnap
.endif

.if ${MK_PPP} != "no"
SUBDIR+=	ppp
.endif

.if ${MK_QUOTAS} != "no"
SUBDIR+=	edquota
SUBDIR+=	quotaon
SUBDIR+=	repquota
.endif

.if ${MK_RCMDS} != "no"
SUBDIR+=	rwhod
.endif

.if ${MK_RCS} != "no"
SUBDIR+=	etcupdate
.endif

.if ${MK_SENDMAIL} != "no"
SUBDIR+=	editmap
SUBDIR+=	mailstats
SUBDIR+=	makemap
SUBDIR+=	praliases
SUBDIR+=	sendmail
.endif

.if ${MK_TCP_WRAPPERS} != "no"
SUBDIR+=	tcpdchk
SUBDIR+=	tcpdmatch
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.if ${MK_TIMED} != "no"
SUBDIR+=	timed
.endif

.if ${MK_TOOLCHAIN} != "no"
SUBDIR+=	config
SUBDIR+=	crunch
.endif

.if ${MK_UNBOUND} != "no"
SUBDIR+=	unbound
.endif

.if ${MK_USB} != "no"
SUBDIR+=	uathload
SUBDIR+=	uhsoctl
SUBDIR+=	usbconfig
SUBDIR+=	usbdump
.endif

.if ${MK_UTMPX} != "no"
SUBDIR+=	ac
SUBDIR+=	lastlogin
SUBDIR+=	utx
.endif

.if ${MK_WIRELESS} != "no"
SUBDIR+=	ancontrol
SUBDIR+=	wlandebug
SUBDIR+=	wpa
.endif

.include <bsd.arch.inc.mk>

SUBDIR:=	${SUBDIR:O}

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
