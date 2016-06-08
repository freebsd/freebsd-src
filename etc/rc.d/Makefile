# $FreeBSD$

.include <bsd.own.mk>

FILES=	DAEMON \
	FILESYSTEMS \
	LOGIN \
	NETWORKING \
	SERVERS \
	abi \
	addswap \
	adjkerntz \
	archdep \
	atm1 \
	atm2 \
	atm3 \
	auditd \
	auditdistd \
	bgfsck \
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
	faith \
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
	initrandom \
	ip6addrctl \
	ipfilter \
	ipfs \
	ipfw \
	ipmon \
	ipnat \
	ipsec \
	${_ipxrouted} \
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
	mroute6d \
	mrouted \
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
	postrandom \
	ppp \
	pppoed \
	pwcheck \
	quota \
	random \
	rarpd \
	rctl \
	resolv \
	rfcomm_pppd_server \
	root \
	route6d \
	routing \
	rpcbind \
	rtadvd \
	rtsold \
	savecore \
	sdpd \
	securelevel \
	serial \
	sppp \
	${_sshd} \
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
	${_unbound} \
	${_utx} \
	var \
	virecover \
	watchdogd \
	ypbind \
	yppasswdd \
	ypserv \
	ypset \
	ypupdated \
	ypxfrd \

.if ${MK_ACCT} != "no"
FILES+=		accounting
.endif

.if ${MK_ACPI} != "no"
FILES+=		power_profile
.endif

.if ${MK_ACPI} != "no" || ${MK_APM} != "no"
FILES+=		powerd
.endif

.if ${MK_AMD} != "no"
FILES+=		amd
.endif

.if ${MK_APM} != "no"
FILES+=	apm
FILES+=	apmd
.endif

.if ${MK_AUTOFS} != "no"
FILES+=		automount
FILES+=		automountd
FILES+=		autounmountd
.endif

.if ${MK_BLUETOOTH} != "no"
_bluetooth=	bluetooth
_bthidd=	bthidd
_hcsecd=	hcsecd
_ubthidhci=	ubthidhci
.endif

.if ${MK_BOOTPARAMD} != "no"
FILES+=		bootparams
.endif

.if ${MK_BSNMP} != "no"
FILES+=		bsnmpd
.endif

.if ${MK_CCD} != "no"
FILES+=		ccd
.endif

.if ${MK_FTP} != "no"
FILES+=		ftpd
.endif

.if ${MK_HAST} != "no"
FILES+=		hastd
.endif

.if ${MK_IPX} != "no"
_ipxrouted=	ipxrouted
.endif

.if ${MK_INETD} != "no"
FILES+=		inetd
.endif

.if ${MK_ISCSI} != "no"
FILES+=		iscsictl
FILES+=		iscsid
.endif

.if ${MK_JAIL} != "no"
FILES+=		jail
.endif

.if ${MK_KERBEROS} != "no"
FILES+=		ipropd_master
FILES+=		ipropd_slave
_kadmind=	kadmind
_kdc=		kdc
_kfd=		kfd
_kpasswdd=	kpasswdd
.endif

.if ${MK_LEGACY_CONSOLE} != "no"
FILES+=		moused
FILES+=		syscons
.endif

.if ${MK_LPR} != "no"
FILES+=		lpd
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
_sshd=		sshd
.endif

.if ${MK_PF} != "no"
FILES+=		ftp-proxy
.endif

.if ${MK_RCMDS} != "no"
FILES+=		rwho
.endif

.if ${MK_ROUTED} != "no"
FILES+=		routed
.endif

.if ${MK_SENDMAIL} != "no"
FILES+=		sendmail
.endif

.if ${MK_TIMED} != "no"
FILES+=		timed
.endif

.if ${MK_UNBOUND} != "no"
_unbound=	local_unbound
.endif

.if ${MK_UTMPX} != "no"
_utx=		utx
.endif

.if ${MK_WIRELESS} != "no"
FILES+=		hostapd
FILES+=		wpa_supplicant
.endif

.if ${MK_ZFS} != "no"
FILES+=		zfs
FILES+=		zvol
.endif

FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

.include <bsd.prog.mk>
