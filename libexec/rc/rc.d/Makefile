# $FreeBSD$

.include <src.opts.mk>

BINDIR=	/etc/rc.d
FILESGROUPS=	BASERC
BASERCPACKAGE=	rc

BASERC=	DAEMON \
	FILESYSTEMS \
	LOGIN \
	NETWORKING \
	SERVERS \
	addswap \
	adjkerntz \
	archdep \
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
	devd \
	devfs \
	devmatch \
	dhclient \
	dmesg \
	dumpon \
	fsck \
	gbde \
	geli \
	geli2 \
	gptboot \
	growfs \
	gssd \
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
	ppp \
	pppoed \
	pwcheck \
	quota \
	random \
	rarpd \
	rctl \
	resolv \
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
	sppp \
	statd \
	static_arp \
	static_ndp \
	stf \
	swap \
	swaplate \
	sysctl \
	syslogd \
	sysvipc \
	tmp \
	ugidfw \
	${_utx} \
	var \
	watchdogd

.if ${MK_NIS} != "no"
BASERC+=	ypbind \
	ypldap \
	yppasswdd \
	ypserv \
	ypset \
	ypupdated \
	ypxfrd
.endif

.if ${MK_ACCT} != "no"
FILESGROUPS+=	ACCT
ACCT+=		accounting
ACCTPACKAGE=	acct
.endif

.if ${MK_ACPI} != "no"
FILESGROUPS+=	ACPI
ACPI=		power_profile
ACPIPACKAGE=	acpi
.endif

.if ${MK_ACPI} != "no" || ${MK_APM} != "no"
BASERC+=	powerd
.endif

.if ${MK_APM} != "no"
FILESGROUPS+=	APM
APM+=		apm
.if ${MACHINE} == "i386"
APM+=		apmd
.endif
APMPACKAGE=	apm
.endif

.if ${MK_AUDIT} != "no"
FILESGROUPS+=	AUDIT
AUDIT+=		auditd
AUDIT+=		auditdistd
.endif

.if ${MK_AUTOFS} != "no"
FILESGROUPS+=	AUFOFS
AUTOFS+=	automount
AUTOFS+=	automountd
AUTOFS+=	autounmountd
AUTOFSPACKAGE=	autofs
.endif

.if ${MK_BLACKLIST} != "no"
_blacklistd+=	blacklistd
.endif

.if ${MK_BLUETOOTH} != "no"
FILESGROUPS+=	BLUETOOTH
BLUETOOTH+=	bluetooth \
		bthidd \
		hcsecd \
		rfcomm_pppd_server \
		sdpd \
		ubthidhci
BLUETOOTHPACKAGE=	bluetooth
.endif

.if ${MK_BOOTPARAMD} != "no"
BASERC+=	bootparams
.endif

.if ${MK_BSNMP} != "no"
FILESGROUPS+=	BSNMP
BSNMP+=		bsnmpd
BSNMPPACKAGE=	bsnmp
.endif

.if ${MK_CCD} != "no"
FILESGROUPS+=	CCD
CCD+=		ccd
CCDPACKAGE=	ccdconfig
.endif

.if ${MK_FTP} != "no"
BASERC+=	ftpd
.endif

.if ${MK_HAST} != "no"
FILESGROUPS+=	HAST
HAST=		hastd
HASTPACKAGE=	hast
.endif

.if ${MK_INETD} != "no"
BASERC+=	inetd
.endif

.if ${MK_IPFILTER} != "no"
FILESGROUPS+=	IPFILTER
IPFILTER+=	ipfilter \
		ipfs \
		ipmon \
		ipnat \
		ippool
IPFILTERPACKAGE=	ipf
.endif

.if ${MK_IPFW} != "no"
FILESGROUP+=	IPFW
IPFW+=	ipfw
.if ${MK_NETGRAPH} != "no"
IPFW+=	ipfw_netflow
.endif
IPFWPACKAGE=	ipfw
.endif

.if ${MK_ISCSI} != "no"
FILESGROUPS+=	ISCSI
ISCSI+=		iscsictl
ISCSI+=		iscsid
ISCSIPACKAGE=	iscsi
.endif

.if ${MK_JAIL} != "no"
FILESGROUPS+=	JAIL
JAIL+=		jail
JAILPACKAGE=	jail
.endif

.if ${MK_LEGACY_CONSOLE} != "no"
BASERC+=	moused
BASERC+=	syscons
.endif

.if ${MK_LPR} != "no"
BASERC+=	lpd
.endif

.if ${MK_KERBEROS} != "no"
BASERC+=	ipropd_master
BASERC+=	ipropd_slave
_kadmind=	kadmind
_kdc=		kdc
_kfd=		kfd
_kpasswdd=	kpasswdd

DIRS+=	VAR_HEMIDAL
VAR_HEMIDAL=	/var/heimdal
VAR_HEMIDAL_MODE=	700
.endif

.if ${MK_MAIL} != "no"
BASERC+=	othermta
.endif

.if ${MK_NS_CACHING} != "no"
_nscd=		nscd
.endif

.if ${MK_NTP} != "no"
BASERC+=	ntpd
.endif

.if ${MK_OFED} != "no"
_opensm=	opensm
.endif

.if ${MK_OPENSSL} != "no"
BASERC+=	keyserv
.endif

.if ${MK_OPENSSH} != "no"
FILESGROUPS+=	SSH
SSH=		sshd
SSHPACKAGE=	ssh
.endif

.if ${MK_PF} != "no"
BASERC+=	ftp-proxy
.endif

.if ${MK_ROUTED} != "no"
BASERC+=	routed
.endif

.if ${MK_SENDMAIL} != "no"
FILESGROUPS+=	SMRCD
SMRCD=		sendmail
SMRCDPACKAGE=	sendmail
.endif

.if ${MK_UNBOUND} != "no"
FILESGROUPS+=	UNBOUND
UNBOUND+=	local_unbound
UNBOUNDPACKAGE=	unbound
.endif

.if ${MK_UTMPX} != "no"
_utx=		utx
.endif

.if ${MK_VI} != "no"
FILESGROUPS+=	VI
VI+=		virecover
VIPACKAGE=	vi
.endif

.if ${MK_WIRELESS} != "no"
FILESGROUPS+=	HOSTAPD WPA
HOSTAPD+=	hostapd
HOSTAPDPACKAGE=	hostapd
WPA+=		wpa_supplicant
WPAPACKAGE=	wpa
.endif

.if ${MK_ZFS} != "no"
FILESGROUPS+=	ZFS
ZFS+=		zfs
ZFS+=		zfsbe
ZFS+=		zfsd
ZFS+=		zvol
.endif

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
