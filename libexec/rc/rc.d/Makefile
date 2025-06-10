.include <src.opts.mk>

CONFDIR=	/etc/rc.d
CONFGROUPS=	CONFS
CONFSPACKAGE=	rc

CONFS=	DAEMON \
	FILESYSTEMS \
	LOGIN \
	NETWORKING \
	SERVERS \
	adjkerntz \
	bgfsck \
	bridge \
	cfumass \
	cleanvar \
	cleartmp \
	ddb \
	defaultroute \
	devfs \
	dmesg \
	dumpon \
	fsck \
	growfs \
	growfs_fstab \
	hostid \
	hostid_save \
	hostname \
	iovctl \
	ip6addrctl \
	ipsec \
	${_kadmind} \
	${_kdc} \
	${_kfd} \
	kld \
	kldxref \
	${_kpasswdd} \
	ldconfig \
	linux \
	local \
	localpkg \
	mixer \
	motd \
	mountcritlocal \
	mountcritremote \
	mountlate \
	mdconfig \
	mdconfig2 \
	msgs \
	netif \
	netoptions \
	netwait \
	noshutdown \
	${_nscd} \
	${_opensm} \
	os-release \
	powerd \
	pppoed \
	pwcheck \
	quota \
	random \
	rarpd \
	rctl \
	root \
	route6d \
	routing \
	rpcbind \
	rtadvd \
	rtsold \
	savecore \
	securelevel \
	serial \
	static_arp \
	static_ndp \
	stf \
	swap \
	swaplate \
	sysctl \
	sysctl_lastload \
	sysvipc \
	tmp \
	ugidfw \
	var \
	var_run \
	watchdogd

CONFGROUPS+=		DEVD
DEVD=			devd
DEVDPACKAGE=		devd

CONFGROUPS+=		DEVMATCH
DEVMATCH=		devmatch
DEVMATCHPACKAGE=	devmatch

CONFGROUPS+=		DHCLIENT
DHCLIENT=		dhclient
DHCLIENTPACKAGE=	dhclient

CONFGROUPS+=		GEOM
GEOM=			geli \
			geli2 \
			gptboot
GEOMPACKAGE=		geom

CONFGROUPS+=		GGATED
GGATED=			ggated
GGATEDPACKAGE=		ggate

CONFGROUPS+=		RESOLVCONF
RESOLVCONF=		resolv
RESOLVCONFPACKAGE=	resolvconf

CONFGROUPS+=		CRON
CRON+=			cron
CRONPACKAGE=		cron

CONFGROUPS+=		CTL
CTL=			ctld
CTLPACKAGE=		ctl

CONFGROUPS+=		NFS
NFS=			lockd \
			mountd \
			nfscbd \
			nfsclient \
			nfsd \
			nfsuserd \
			statd
NFSPACKAGE=		nfs

CONFGROUPS+=		NEWSYSLOG
NEWSYSLOG=		newsyslog
NEWSYSLOGPACKAGE=	newsyslog

CONFGROUPS+=		SYSLOGD
SYSLOGD=		syslogd
SYSLOGDPACKAGE=		syslogd

CONFGROUPS+=		RCMDS
RCMDS=			rwho
RCMDSPACKAGE=		rcmds

.if ${MK_ACCT} != "no" || ${MK_UTMPX} != "no"
CONFGROUPS+=	ACCT
ACCTPACKAGE=	acct
.if ${MK_ACCT} != "no"
ACCT+=		accounting
.endif
.if ${MK_UTMPX} != "no"
ACCT+=		utx
.endif
.endif

.if ${MK_ACPI} != "no"
CONFGROUPS+=	ACPI
ACPI=		power_profile
ACPIPACKAGE=	acpi
.endif

.if ${MK_APM} != "no"
CONFGROUPS+=	APM
APM+=		apm
.if ${MACHINE} == "i386"
APM+=		apmd
.endif
APMPACKAGE=	apm
.endif

.if ${MK_AUDIT} != "no"
CONFGROUPS+=	AUDIT
AUDIT+=		auditd
AUDIT+=		auditdistd
AUDITPACKAGE=	audit
.endif

.if ${MK_AUTOFS} != "no"
CONFGROUPS+=	AUTOFS
AUTOFS=		automount \
		automountd \
		autounmountd
AUTOFSPACKAGE=	autofs
.endif

.if ${MK_BLACKLIST} != "no"
CONFGROUPS+=	BLOCKLIST
BLOCKLIST=	blacklistd
BLOCKLISTPACKAGE=blocklist
.endif

.if ${MK_BLUETOOTH} != "no"
CONFGROUPS+=	BLUETOOTH
BLUETOOTH+=	bluetooth \
		bthidd \
		hcsecd \
		rfcomm_pppd_server \
		sdpd \
		ubthidhci
BLUETOOTHPACKAGE=	bluetooth
.endif

.if ${MK_BOOTPARAMD} != "no"
CONFS+=		bootparams
.endif

.if ${MK_BSNMP} != "no"
CONFGROUPS+=	BSNMP
BSNMP+=		bsnmpd
BSNMPPACKAGE=	bsnmp
.endif

.if ${MK_CCD} != "no"
CONFGROUPS+=	CCD
CCD=		ccd
CCDPACKAGE=	ccdconfig
.endif

.if ${MK_FTP} != "no"
CONFGROUPS+=	FTPD
FTPD=		ftpd
FTPDPACKAGE=	ftpd
.endif

.if ${MK_GSSAPI} != "no"
CONFGROUPS+=	GSSD
GSSD=		gssd
GSSDPACKAGE=	kerberos
.endif

.if ${MK_HAST} != "no"
CONFGROUPS+=	HAST
HAST=		hastd
HASTPACKAGE=	hast
.endif

.if ${MK_INETD} != "no"
CONFGROUPS+=	INETD
INETD=		inetd
INETDPACKAGE=	inetd
.endif

.if ${MK_IPFILTER} != "no"
CONFGROUPS+=	IPF
IPF=		ipfilter \
		ipfs \
		ipmon \
		ipnat \
		ippool
IPFPACKAGE=	ipf
.endif

.if ${MK_IPFW} != "no"
CONFGROUPS+=	IPFW
IPFW=		ipfw dnctl
.if ${MK_NETGRAPH} != "no"
IPFW+=		ipfw_netflow
.endif
IPFWPACKAGE=	ipfw

# natd is only built when ipfw is built
CONFGROUPS+=	NATD
NATD+=		natd
NATDPACKAGE=	natd
.endif

.if ${MK_ISCSI} != "no"
CONFGROUPS+=	ISCSI
ISCSI=		iscsictl \
		iscsid
ISCSIPACKAGE=	iscsi
.endif

.if ${MK_JAIL} != "no"
CONFGROUPS+=	JAIL
JAIL+=		jail
JAILPACKAGE=	jail
.endif

.if ${MK_LEGACY_CONSOLE} != "no"
CONFGROUPS+=	CONSOLE
CONSOLE+=	moused
CONSOLE+=	syscons
CONSOLEPACKAGE=	console-tools
.endif

.if ${MK_LPR} != "no"
CONFGROUPS+=	LP
LP+=		lpd
LPPACKAGE=	lp
.endif

.if ${MK_KERBEROS} != "no"
CONFS+=		ipropd_master
CONFS+=		ipropd_slave
_kadmind=	kadmind
_kdc=		kdc
_kfd=		kfd
_kpasswdd=	kpasswdd

DIRS+=	VAR_HEMIDAL
VAR_HEMIDAL=	/var/heimdal
VAR_HEMIDAL_MODE=	700
.endif

.if ${MK_NIS} != "no"
CONFGROUPS+=	YP
YP=		ypbind \
		ypldap \
		yppasswdd \
		ypserv \
		ypset \
		ypupdated \
		ypxfrd \
		nisdomain
YPPACKAGE=	yp
.endif

.if ${MK_NS_CACHING} != "no"
_nscd=		nscd
.endif

.if ${MK_NTP} != "no"
CONFGROUPS+=	NTP
NTP+=		ntpd \
		ntpdate
NTPPACKAGE=	ntp
.endif

.if ${MK_OFED_EXTRA} != "no"
_opensm=	opensm
.endif

.if ${MK_OPENSSL} != "no" && ${MK_OPENSSL_KTLS} != "no"
CONFS+=		tlsclntd \
		tlsservd
.endif

.if ${MK_OPENSSH} != "no"
CONFGROUPS+=	SSH
SSH=		sshd
SSHPACKAGE=	ssh
.endif

.if ${MK_PF} != "no"
CONFGROUPS+=	PF
PF=		pf \
		pflog \
		pfsync \
		ftp-proxy
PFPACKAGE=	pf
.endif

.if ${MK_PPP} != "no"
CONFGROUPS+=	PPP
PPP=		ppp
PPPPACKAGE=	ppp
.endif

.if ${MK_ROUTED} != "no"
CONFS+=		routed
.endif

.if ${MK_SENDMAIL} != "no"
CONFGROUPS+=	SMRCD
SMRCD=		sendmail
SMRCDPACKAGE=	sendmail
.endif

.if ${MK_NUAGEINIT} != "no"
CONFGROUPS+=	NIUAGEINIT
NIUAGEINIT=		nuageinit \
			nuageinit_user_data_script
NIUAGEINITPACKAGE=	nuageinit
.endif

.if ${MK_UNBOUND} != "no"
CONFGROUPS+=	UNBOUND
UNBOUND+=	local_unbound
UNBOUNDPACKAGE=	unbound
.endif

.if ${MK_VI} != "no"
CONFGROUPS+=	VI
VI+=		virecover
VIPACKAGE=	vi
.endif

.if ${MK_WIRELESS} != "no"
CONFGROUPS+=	HOSTAPD
HOSTAPD=	hostapd
HOSTAPDPACKAGE=	hostapd

CONFGROUPS+=	WPA
WPA=		wpa_supplicant
WPAPACKAGE=	wpa
.endif

.if ${MK_ZFS} != "no"
CONFGROUPS+=	ZFS
ZFS+=		zfs
ZFS+=		zfsbe
ZFS+=		zfsd
ZFS+=		zfskeys
ZFS+=		zpool
ZFS+=		zpoolreguid
ZFS+=		zpoolupgrade
ZFS+=		zvol
ZFSPACKAGE=	zfs
.endif

.for fg in ${CONFGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
