# $FreeBSD$

.include <src.opts.mk>

BINDIR=		/etc/rc.d
FILESGROUPS=	FILES

FILES=	DAEMON \
	FILESYSTEMS \
	LOGIN \
	NETWORKING \
	SERVERS \
	abi \
	addswap \
	adjkerntz \
	archdep \
	auditd \
	auditdistd \
	bgfsck \
	${_blacklistd} \
	${_bluetooth} \
	bridge \
	${_bthidd} \
	cleanvar \
	cleartmp \
	cron \
	ctld \
	ddb \
	defaultroute \
	devd \
	devfs \
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
	${_hcsecd} \
	hostid \
	hostid_save \
	hostname \
	iovctl \
	ip6addrctl \
	ipfilter \
	ipfs \
	ipfw \
	ipmon \
	ipnat \
	ipsec \
	${_kadmind} \
	${_kdc} \
	${_kfd} \
	kld \
	kldxref \
	${_kpasswdd} \
	ldconfig \
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
	nsswitch \
	ntpdate \
	${_opensm} \
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
	tmp \
	${_ubthidhci} \
	ugidfw \
	${_utx} \
	var \
	watchdogd \
	ypbind \
	ypldap \
	yppasswdd \
	ypserv \
	ypset \
	ypupdated \
	ypxfrd

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
FILES+=		powerd
.endif

.if ${MK_AMD} != "no"
FILESGROUPS+=	AMD
AMD+=		amd
AMDPACKAGE=	amd
.endif

.if ${MK_APM} != "no"
FILESGROUPS+=	APM
APM+=		apm
APM+=		apmd
APMPACKAGE=	apm
.endif

.if ${MK_ATM} != "no"
FILESGROUPS+=	ATM
ATM+=		atm1
ATM+=		atm2
ATM+=		atm3
ATMPACKAGE=	atm
.endif

.if ${MK_AUTOFS} != "no"
FILES+=		automount
FILES+=		automountd
FILES+=		autounmountd
.endif

.if ${MK_BLACKLIST_SUPPORT} != "no"
_blacklistd+=	blacklistd
.endif

.if ${MK_BLUETOOTH} != "no"
_bluetooth=	bluetooth
_bthidd=	bthidd
_hcsecd=	hcsecd
FILES+=		rfcomm_pppd_server
FILES+=		sdpd
_ubthidhci=	ubthidhci
.endif

.if ${MK_BOOTPARAMD} != "no"
FILES+=		bootparams
.endif

.if ${MK_BSNMP} != "no"
FILESGROUPS+=	BSNMP
BSNMP+=		bsnmpd
BSNMPPACKAGE=	bsnmp
.endif

.if ${MK_CCD} != "no"
FILES+=		ccd
.endif

.if ${MK_FTP} != "no"
FILES+=		ftpd
.endif

.if ${MK_HAST} != "no"
FILESGROUPS+=	HAST
HAST=		hastd
HASTPACKAGE=	hast
.endif

.if ${MK_INETD} != "no"
FILES+=		inetd
.endif

.if ${MK_ISCSI} != "no"
FILES+=		iscsictl
FILES+=		iscsid
.endif

.if ${MK_JAIL} != "no"
FILESGROUPS+=	JAIL
JAIL+=		jail
JAILPACKAGE=	jail
.endif

.if ${MK_LEGACY_CONSOLE} != "no"
FILES+=		moused
FILES+=		syscons
.endif

.if ${MK_LPR} != "no"
FILES+=		lpd
.endif

.if ${MK_KERBEROS} != "no"
FILES+=		ipropd_master
FILES+=		ipropd_slave
_kadmind=	kadmind
_kdc=		kdc
_kfd=		kfd
_kpasswdd=	kpasswdd
.endif

.if ${MK_MAIL} != "no"
FILES+=		othermta
.endif

.if ${MK_NS_CACHING} != "no"
_nscd=		nscd
.endif

.if ${MK_NTP} != "no"
FILES+=		ntpd
.endif

.if ${MK_OFED} != "no"
_opensm=	opensm
.endif

.if ${MK_OPENSSL} != "no"
FILES+=		keyserv
.endif

.if ${MK_OPENSSH} != "no"
FILESGROUPS+=	SSH
SSH=		sshd
SSHPACKAGE=	ssh
.endif

.if ${MK_PF} != "no"
FILES+=		ftp-proxy
.endif

.if ${MK_RCMDS} != "no"
FILESGROUPS+=	RCMDS
RCMDS+=		rwho
RCMDSPACKAGE=	rcmds
.endif

.if ${MK_ROUTED} != "no"
FILES+=		routed
.endif

.if ${MK_SENDMAIL} != "no"
FILESGROUPS+=	SMRCD
SMRCD=		sendmail
SMRCDPACKAGE=	sendmail
.endif

.if ${MK_TIMED} != "no"
FILES+=		timed
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
FILES+=		hostapd
FILES+=		wpa_supplicant
.endif

.if ${MK_ZFS} != "no"
FILESGROUPS+=	ZFS
ZFS+=		zfs
ZFS+=		zfsd
ZFS+=		zvol
ZFSPACKAGE=	zfs
.endif

.for fg in ${FILESGROUPS}
${fg}MODE?=	${BINMODE}
.endfor

.include <bsd.prog.mk>
