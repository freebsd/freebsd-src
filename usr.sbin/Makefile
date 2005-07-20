#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $FreeBSD$

# XXX MISSING:		mkproto
SUBDIR=	ac \
	accton \
	${_acpi} \
	adduser \
	amd \
	ancontrol \
	${_apm} \
	${_apmd} \
	${_arlcontrol} \
	arp \
	${_asf} \
	${_atm} \
	${_authpf} \
	${_bluetooth} \
	${_boot0cfg} \
	${_boot98cfg} \
	bootparamd \
	bsnmpd \
	${_btxld} \
	burncd \
	cdcontrol \
	chkgrp \
	chown \
	chroot \
	ckdist \
	config \
	cron \
	crunch \
	ctm \
	daemon \
	dconschat \
	devinfo \
	digictl \
	diskinfo \
	${_dnssec-keygen} \
	${_dnssec-signzone} \
	${_editmap} \
	edquota \
	${_eeprom} \
	${_elf2exe} \
	extattr \
	extattrctl \
	faithd \
	fdcontrol \
	fdformat \
	fdread \
	fdwrite \
	flowctl \
	fwcontrol \
	getfmac \
	getpmac \
	gstat \
	${_i4b} \
	ifmcstat \
	inetd \
	iostat \
	ip6addrctl \
	IPXrouted \
	jail \
	jexec \
	jls \
	kbdcontrol \
	kbdmap \
	${_keyserv} \
	${_kgmon} \
	${_kgzip} \
	kldxref \
	lastlogin \
	${_lpr} \
	${_lptcontrol} \
	${_mailstats} \
	mailwrapper \
	${_makemap} \
	manctl \
	memcontrol \
	mergemaster \
	mixer \
	${_mld6query} \
	mlxcontrol \
	mountd \
	${_mount_nwfs} \
	mount_portalfs \
	${_mount_smbfs} \
	moused \
	${_mptable} \
	mrouted \
	mtest \
	mtree \
	${_named} \
	${_named.reload} \
	${_named-checkconf} \
	${_named-checkzone} \
	${_ndiscvt} \
	ndp \
	newsyslog \
	nfsd \
	ngctl \
	nghook \
	nologin \
	ntp \
	${_ofwdump} \
	${_pccard} \
	pciconf \
	${_pcvt} \
	periodic \
	pkg_install \
	pmccontrol \
	pmcstat \
	${_pnpinfo} \
	powerd \
	ppp \
	${_pppctl} \
	pppd \
	pppstats \
	${_praliases} \
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	quotaon \
	rarpd \
	raycontrol \
	repquota \
	${_rip6query} \
	rmt \
	${_rndc} \
	${_rndc-confgen} \
	${_route6d} \
	rpcbind \
	rpc.lockd \
	rpc.statd \
	rpc.umntall \
	${_rpc.yppasswdd} \
	${_rpc.ypupdated} \
	${_rpc.ypxfrd} \
	rrenumd \
	rtadvd \
	rtprio \
	rtsold \
	rwhod \
	sa \
	${_sendmail} \
	setfmac \
	setkey \
	setpmac \
	${_sicontrol} \
	sliplogin \
	slstat \
	smbmsg \
	snapinfo \
	${_spkrtest} \
	spray \
	${_sysinstall} \
	syslogd \
	tcpdchk \
	tcpdmatch \
	tcpdrop \
	tcpdump \
	timed \
	traceroute \
	${_traceroute6} \
	trpt \
	tzsetup \
	ugidfw \
	${_usbd} \
	${_usbdevs} \
	vidcontrol \
	vipw \
	vnconfig \
	watch \
	watchdogd \
	wicontrol \
	wpa \
	${_wlconfig} \
	${_ypbind} \
	${_yp_mkdb} \
	${_yppoll} \
	${_yppush} \
	${_ypserv} \
	${_ypset} \
	zic \
	${_zzz}

.if ${MACHINE_ARCH} != "arm"
_sysinstall=	sysinstall
.endif

.if !defined(NO_ATM)
_atm=		atm
.endif

.if !defined(NO_BIND)
.if !defined(NO_BIND_DNSSEC) && !defined(NO_CRYPT)
_dnssec-keygen=		dnssec-keygen
_dnssec-signzone=	dnssec-signzone
.endif
.if !defined(NO_BIND_NAMED)
_named=			named
_named.reload=		named.reload
_named-checkconf=	named-checkconf
_named-checkzone=	named-checkzone
_rndc=			rndc
_rndc-confgen=		rndc-confgen
.endif
.endif

.if !defined(NO_BLUETOOTH)
_bluetooth=	bluetooth
.endif

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
_keyserv=	keyserv
.endif

.if !defined(NO_INET6)
_mld6query=	mld6query
_rip6query=	rip6query
_route6d=	route6d
_traceroute6=	traceroute6
.endif

.if !defined(NO_NIS)
_rpc.yppasswdd=	rpc.yppasswdd
_rpc.ypupdated=	rpc.ypupdated
_rpc.ypxfrd=	rpc.ypxfrd
_ypbind=	ypbind
_yp_mkdb=	yp_mkdb
_yppoll=	yppoll
_yppush=	yppush
_ypserv=	ypserv
_ypset=		ypset
.endif

.if !defined(NO_PF) && !defined(NO_AUTHPF)
_authpf=	authpf
.endif

.if !defined(NO_LPR)
_lpr=		lpr
.endif

.if !defined(NO_SENDMAIL)
_editmap=	editmap
_mailstats=	mailstats
_makemap=	makemap
_praliases=	praliases
_sendmail=	sendmail
.endif

.if !defined(NO_USB)
_usbd=		usbd
_usbdevs=	usbdevs
.endif

.if ${MACHINE_ARCH} == "arm"
_kgmon=		kgmon
.endif

.if ${MACHINE_ARCH} == "i386"
_apm=		apm
_apmd=		apmd
_asf=		asf
_btxld=		btxld
.if !defined(NO_I4B)
_i4b=		i4b
.endif
_kgmon=		kgmon
_kgzip=		kgzip
_lptcontrol=	lptcontrol
_mount_nwfs=	mount_nwfs
_mount_smbfs=	mount_smbfs
_mptable=	mptable
_ndiscvt=	ndiscvt
_pccard=	pccard
_pnpinfo=	pnpinfo
.if !defined(NO_LIBPTHREAD)
_pppctl=	pppctl
.endif
_sicontrol=	sicontrol
_spkrtest=	spkrtest
_zzz=		zzz
.if ${MACHINE} == "i386" 
.if !defined(NO_ACPI)
_acpi=		acpi
.endif
_arlcontrol=	arlcontrol
_boot0cfg=	boot0cfg
_pcvt=		pcvt
_wlconfig=	wlconfig
.elif ${MACHINE} == "pc98"
_boot98cfg=	boot98cfg
.endif
.endif

.if ${MACHINE_ARCH} == "alpha"
_elf2exe=	elf2exe
_pnpinfo=	pnpinfo
.if !defined(NO_LIBC_R)
_pppctl=	pppctl
.endif
.endif

# kgzip: builds, but missing support files
# mptable: broken (not 64 bit clean)
# pnpinfo: crashes (not really useful anyway)
.if ${MACHINE_ARCH} == "amd64"
.if !defined(NO_ACPI)
_acpi=		acpi
.endif
_asf=		asf
_boot0cfg=	boot0cfg
_btxld=		btxld
_kgmon=		kgmon
_lptcontrol=	lptcontrol
_mount_nwfs=	mount_nwfs
_mount_smbfs=	mount_smbfs
_mptable=	mptable
_ndiscvt=	ndiscvt
.if !defined(NO_LIBPTHREAD)
_pppctl=	pppctl
.endif
_sicontrol=	sicontrol
_spkrtest=	spkrtest
_zzz=		zzz
.endif

.if ${MACHINE_ARCH} == "ia64"
.if !defined(NO_ACPI)
_acpi=		acpi
.endif
_kgmon=		kgmon
.if !defined(NO_LIBPTHREAD)
_pppctl=	pppctl
.endif
_zzz=		zzz
.endif

.if ${MACHINE_ARCH} == "sparc64"
_eeprom=	eeprom
_ofwdump=	ofwdump
.if !defined(NO_LIBC_R)
_pppctl=	pppctl
.endif
.endif

.include <bsd.subdir.mk>
