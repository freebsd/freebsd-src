
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
	${_blacklistd} \
	bridge \
	cfumass \
	cleanvar \
	cleartmp \
	cron \
	ctld \
	ddb \
	defaultroute \
	devfs \
	dmesg \
	dumpon \
	fsck \
	gbde \
	geli \
	geli2 \
	gptboot \
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
	lockd \
	mixer \
	motd \
	mountcritlocal \
	mountcritremote \
	mountlate \
	mdconfig \
	mdconfig2 \
	mountd \
	msgs \
	natd \
	netif \
	netoptions \
	netwait \
	newsyslog \
	nfsclient \
	nfscbd \
	nfsd \
	nfsuserd \
	nisdomain \
	${_nscd} \
	ntpdate \
	${_opensm} \
	os-release \
	pf \
	pflog \
	pfsync \
	powerd \
	ppp \
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
	rwho \
	savecore \
	securelevel \
	serial \
	statd \
	static_arp \
	static_ndp \
	stf \
	swap \
	swaplate \
	sysctl \
	sysctl_lastload \
	syslogd \
	sysvipc \
	tmp \
	ugidfw \
	${_utx} \
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

CONFGROUPS+=		GGATED
GGATED=			ggated
GGATEDPACKAGE=		ggate

CONFGROUPS+=		RESOLVCONF
RESOLVCONF=		resolv
RESOLVCONFPACKAGE=	resolvconf

.if ${MK_NIS} != "no"
CONFS+=	ypbind \
	ypldap \
	yppasswdd \
	ypserv \
	ypset \
	ypupdated \
	ypxfrd
.endif

.if ${MK_ACCT} != "no"
CONFGROUPS+=	ACCT
ACCT+=		accounting
ACCTPACKAGE=	acct
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
AUDITPACKAGE=	rc
.endif

.if ${MK_AUTOFS} != "no"
CONFS+=		automount
CONFS+=		automountd
CONFS+=		autounmountd
.endif

.if ${MK_BLACKLIST} != "no"
_blacklistd+=	blacklistd
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
CONFS+=		ccd
.endif

.if ${MK_FTP} != "no"
CONFS+=		ftpd
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
CONFS+=		inetd
.endif

.if ${MK_IPFILTER} != "no"
CONFS+=	ipfilter \
	ipfs \
	ipmon \
	ipnat \
	ippool
.endif

.if ${MK_IPFW} != "no"
CONFS+=		ipfw \
		dnctl
.if ${MK_NETGRAPH} != "no"
CONFS+=		ipfw_netflow
.endif
.endif

.if ${MK_ISCSI} != "no"
CONFS+=		iscsictl
CONFS+=		iscsid
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

.if ${MK_NS_CACHING} != "no"
_nscd=		nscd
.endif

.if ${MK_NTP} != "no"
CONFS+=		ntpd
.endif

.if ${MK_OFED} != "no"
_opensm=	opensm
.endif

.if ${MK_OPENSSL} != "no"
CONFS+=		keyserv
.if ${MK_OPENSSL_KTLS} != "no"
CONFS+=		tlsclntd \
		tlsservd
.endif
.endif

.if ${MK_OPENSSH} != "no"
CONFGROUPS+=	SSH
SSH=		sshd
SSHPACKAGE=	ssh
.endif

.if ${MK_PF} != "no"
CONFS+=		ftp-proxy
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
NIUAGEINIT=		nuageinit
NIUAGEINITPACKAGE=	nuageinit
.endif

.if ${MK_UNBOUND} != "no"
CONFGROUPS+=	UNBOUND
UNBOUND+=	local_unbound
UNBOUNDPACKAGE=	unbound
.endif

.if ${MK_UTMPX} != "no"
_utx=		utx
.endif

.if ${MK_VI} != "no"
CONFGROUPS+=	VI
VI+=		virecover
VIPACKAGE=	vi
.endif

.if ${MK_WIRELESS} != "no"
CONFS+=		hostapd
CONFS+=		wpa_supplicant
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
ZFSPACKAGE=	rc
.endif

.for fg in ${CONFGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
