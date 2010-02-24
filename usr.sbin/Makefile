#	From: @(#)Makefile	5.20 (Berkeley) 6/12/93
# $FreeBSD$

.include <bsd.own.mk>

# XXX MISSING:		mkproto
SUBDIR=	${_ac} \
	${_accton} \
	${_acpi} \
	adduser \
	${_amd} \
	${_ancontrol} \
	${_apm} \
	${_apmd} \
	arp \
	${_asf} \
	${_atm} \
	${_audit} \
	${_auditd} \
	${_auditreduce} \
	${_authpf} \
	${_bluetooth} \
	${_boot0cfg} \
	${_boot98cfg} \
	bootparamd \
	${_bsnmpd} \
	${_btxld} \
	burncd \
	cdcontrol \
	chkgrp \
	chown \
	chroot \
	ckdist \
	clear_locks \
	${_config} \
	${_cpucontrol} \
	crashinfo \
	cron \
	${_crunch} \
	${_ctm} \
	daemon \
	dconschat \
	devinfo \
	digictl \
	diskinfo \
	${_dnssec-dsfromkey} \
	${_dnssec-keyfromlabel} \
	${_dnssec-keygen} \
	${_dnssec-signzone} \
	dumpcis \
	${_editmap} \
	${_edquota} \
	${_eeprom} \
	extattr \
	extattrctl \
	${_faithd} \
	${_fdcontrol} \
	${_fdformat} \
	${_fdread} \
	${_fdwrite} \
	fifolog \
	${_flowctl} \
	${_freebsd-update} \
	${_ftp-proxy} \
	fwcontrol \
	getfmac \
	getpmac \
	gstat \
	${_gssd} \
	i2c \
	ifmcstat \
	inetd \
	iostat \
	${_ip6addrctl} \
	${_ipfwpcap} \
	${_IPXrouted} \
	${_jail} \
	${_jexec} \
	${_jls} \
	${_kbdcontrol} \
	${_kbdmap} \
	${_keyserv} \
	${_kgmon} \
	${_kgzip} \
	kldxref \
	lastlogin \
	${_lmcconfig} \
	${_lpr} \
	${_lptcontrol} \
	${_mailstats} \
	mailwrapper \
	makefs \
	${_makemap} \
	manctl \
	memcontrol \
	mergemaster \
	mfiutil \
	mixer \
	${_mld6query} \
	mlxcontrol \
	mountd \
	${_mount_nwfs} \
	mount_portalfs \
	${_mount_smbfs} \
	${_moused} \
	${_mptable} \
	mptutil \
	mtest \
	mtree \
	${_named} \
	${_named-checkconf} \
	${_named-checkzone} \
	${_named.reload} \
	${_ndiscvt} \
	${_ndp} \
	newsyslog \
	nfscbd \
	nfsd \
	nfsdumpstate \
	nfsrevoke \
	nfsuserd \
	${_ngctl} \
	${_nghook} \
	nologin \
	${_nscd} \
	${_ntp} \
	${_nvram} \
	${_ofwdump} \
	pciconf \
	periodic \
	${_pkg_install} \
	${_pmcannotate} \
	${_pmccontrol} \
	${_pmcstat} \
	${_pnpinfo} \
	${_portsnap} \
	powerd \
	${_ppp} \
	${_pppctl} \
	${_praliases} \
	${_praudit} \
	procctl \
	pstat \
	pw \
	pwd_mkdb \
	quot \
	${_quotaon} \
	rarpd \
	${_repquota} \
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
	${_rrenumd} \
	${_rtadvd} \
	rtprio \
	${_rtsold} \
	${_rwhod} \
	${_sa} \
	${_sade} \
	${_sendmail} \
	service \
	setfib \
	setfmac \
	setpmac \
	${_sicontrol} \
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
	${_uathload} \
	ugidfw \
	${_uhsoctl} \
	${_usbdevs} \
	${_usbconfig} \
	${_vidcontrol} \
	vipw \
	wake \
	watch \
	watchdogd \
	${_wlandebug} \
	${_wlconfig} \
	${_wpa} \
	${_ypbind} \
	${_yp_mkdb} \
	${_yppoll} \
	${_yppush} \
	${_ypserv} \
	${_ypset} \
	zic \
	${_zzz}

# NB: keep these sorted by MK_* knobs

.if ${MK_ACCT} != "no"
_ac=		ac
_accton=	accton
_sa=		sa
.endif

.if ${MK_AMD} != "no"
_amd=		amd
.endif

.if ${MK_AUDIT} != "no"
_audit=		audit
_auditd=	auditd
_auditreduce=	auditreduce
_praudit=	praudit
.endif

.if ${MK_AUTHPF} != "no"
_authpf=	authpf
.endif

.if ${MK_BIND_DNSSEC} != "no" && ${MK_OPENSSL} != "no"
_dnssec-dsfromkey=	dnssec-dsfromkey
_dnssec-keyfromlabel=	dnssec-keyfromlabel
_dnssec-keygen=		dnssec-keygen
_dnssec-signzone=	dnssec-signzone
.endif
.if ${MK_BIND_NAMED} != "no"
_named=			named
_named-checkconf=	named-checkconf
_named-checkzone=	named-checkzone
_named.reload=		named.reload
_rndc=			rndc
_rndc-confgen=		rndc-confgen
.endif

.if ${MK_BLUETOOTH} != "no"
_bluetooth=	bluetooth
.endif

.if ${MK_BSNMP} != "no"
_bsnmpd=	bsnmpd
.endif

.if ${MK_CTM} != "no"
_ctm=		ctm
.endif

.if ${MK_FLOPPY} != "no"
_fdcontrol=	fdcontrol
_fdformat=	fdformat
_fdread=	fdread
_fdwrite=	fdwrite
.endif

.if ${MK_FREEBSD_UPDATE} != "no"
_freebsd-update= freebsd-update
.endif

.if ${MK_GSSAPI} != no
_gssd=		gssd
.endif

.if ${MK_INET6} != "no"
_faithd=	faithd
_ip6addrctl=	ip6addrctl
_mld6query=	mld6query
_ndp=		ndp
_rip6query=	rip6query
_route6d=	route6d
_rrenumd=	rrenumd
_rtadvd=	rtadvd
_rtsold=	rtsold
_traceroute6=	traceroute6
.endif

.if ${MK_IPFW} != "no"
_ipfwpcap=	ipfwpcap
.endif

.if ${MK_IPX} != "no"
_IPXrouted=	IPXrouted
.endif

.if ${MK_JAIL} != "no"
_jail=		jail
_jexec=		jexec
_jls=		jls
.endif

# XXX MK_SYSCONS
# XXX is moused w/ usb useful?
.if ${MK_LEGACY_CONSOLE} != "no"
_kbdcontrol=	kbdcontrol
_kbdmap=	kbdmap
_moused=	moused
_vidcontrol=	vidcontrol
.endif

.if ${MK_LIBTHR} != "no" || ${MK_LIBPTHREAD} != "no"
.if ${MK_PPP} != "no"
_pppctl=	pppctl
.endif
.if ${MK_NS_CACHING} != "no"
_nscd=		nscd
.endif
.endif

.if ${MK_LPR} != "no"
_lpr=		lpr
.endif

.if ${MK_NETGRAPH} != "no"
_flowctl=	flowctl
_lmcconfig=	lmcconfig
_ngctl=		ngctl
_nghook=	nghook
.endif

.if ${MK_NIS} != "no"
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

.if ${MK_NTP} != "no"
_ntp=		ntp
.endif

.if ${MK_OPENSSL} != "no"
_keyserv=	keyserv
.endif

.if ${MK_PF} != "no"
_ftp-proxy=	ftp-proxy
.endif

.if ${MK_PKGTOOLS} != "no"
_pkg_install=	pkg_install
.endif

# XXX MK_TOOLCHAIN?
.if ${MK_PMC} != "no"
_pmcannotate=	pmcannotate
_pmccontrol=	pmccontrol
_pmcstat=	pmcstat
.endif

.if ${MK_PORTSNAP} != "no"
_portsnap=	portsnap
.endif

.if ${MK_PPP} != "no"
_ppp=		ppp
#_pppctl handled below
.endif

.if ${MK_QUOTAS} != "no"
_edquota=	edquota
_quotaon=	quotaon
_repquota=	repquota
.endif

.if ${MK_RCMDS} != "no"
_rwhod=		rwhod
.endif

.if ${MK_SENDMAIL} != "no"
_editmap=	editmap
_mailstats=	mailstats
_makemap=	makemap
_praliases=	praliases
_sendmail=	sendmail
.endif

.if ${MK_SYSINSTALL} != "no"
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "sparc64"
_sade=		sade
.endif
.if ${MACHINE_ARCH} != "arm" && ${MACHINE_ARCH} != "mips"
_sysinstall=	sysinstall
.endif
.endif

.if ${MK_TOOLCHAIN} != "no"
_config=	config
_crunch=	crunch
.endif

.if ${MK_USB} != "no"
.if ${MACHINE_ARCH} != "ia64"
_uathload=	uathload
.endif
_uhsoctl=	uhsoctl
#_usbdevs=	usbdevs
_usbconfig=	usbconfig
.endif

.if ${MK_WIRELESS} != "no"
_ancontrol=	ancontrol
_wlandebug=	wlandebug
_wpa=		wpa
.endif

.if ${MACHINE_ARCH} == "arm"
_kgmon=		kgmon
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MK_APM} != "no"
_apm=		apm
_apmd=		apmd
.endif
_asf=		asf
.if ${MK_TOOLCHAIN} != "no"
_btxld=		btxld
.endif
_cpucontrol=	cpucontrol
_kgmon=		kgmon
_kgzip=		kgzip
_lptcontrol=	lptcontrol
.if ${MK_NCP} != "no"
_mount_nwfs=	mount_nwfs
.endif
_mount_smbfs=	mount_smbfs
_mptable=	mptable
.if ${MK_NDIS} != "no"
_ndiscvt=	ndiscvt
.endif
_pnpinfo=	pnpinfo
_sicontrol=	sicontrol
_spkrtest=	spkrtest
_zzz=		zzz
.if ${MACHINE} == "i386"
.if ${MK_ACPI} != "no"
_acpi=		acpi
.endif
_boot0cfg=	boot0cfg
.if ${MK_WIRELESS} != "no"
_wlconfig=	wlconfig
.endif
.elif ${MACHINE} == "pc98"
_boot98cfg=	boot98cfg
.endif
.endif

# kgzip: builds, but missing support files
# mptable: broken (not 64 bit clean)
# pnpinfo: crashes (not really useful anyway)
.if ${MACHINE_ARCH} == "amd64"
.if ${MK_ACPI} != "no"
_acpi=		acpi
.endif
.if ${MK_APM} != "no"
_apm=		apm
.endif
_asf=		asf
_boot0cfg=	boot0cfg
.if ${MK_TOOLCHAIN} != "no"
_btxld=		btxld
.endif
_cpucontrol=	cpucontrol
_kgmon=		kgmon
_lptcontrol=	lptcontrol
.if ${MK_NCP} != "no"
_mount_nwfs=	mount_nwfs
.endif
_mount_smbfs=	mount_smbfs
_mptable=	mptable
.if ${MK_NDIS} != "no"
_ndiscvt=	ndiscvt
.endif
_sicontrol=	sicontrol
_spkrtest=	spkrtest
_zzz=		zzz
.endif

.if ${MACHINE_ARCH} == "ia64"
.if ${MK_ACPI} != "no"
_acpi=		acpi
.endif
_kgmon=		kgmon
_mount_smbfs=	mount_smbfs
_zzz=		zzz
.endif

.if ${MACHINE_ARCH} == "powerpc"
_mount_smbfs=	mount_smbfs
_nvram=		nvram
_ofwdump=	ofwdump
.endif

.if ${MACHINE_ARCH} == "sparc64"
_eeprom=	eeprom
_mount_smbfs=	mount_smbfs
_ofwdump=	ofwdump
.endif

.include <bsd.subdir.mk>
