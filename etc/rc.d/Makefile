# $FreeBSD$

.include <bsd.own.mk>

FILES=	DAEMON \
	FILESYSTEMS \
	LOGIN \
	NETWORKING \
	SERVERS \
	abi \
	accounting \
	addswap \
	adjkerntz \
	amd \
	apm \
	apmd \
	archdep \
	atm1 \
	atm2 \
	atm3 \
	auditd \
	auditdistd \
	automount \
	automountd \
	autounmountd \
	bgfsck \
	${_bluetooth} \
	bootparams \
	bridge \
	bsnmpd \
	${_bthidd} \
	ccd \
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
	ftp-proxy \
	ftpd \
	gbde \
	geli \
	geli2 \
	gptboot \
	gssd \
	hastd \
	${_hcsecd} \
	hostapd \
	hostid \
	hostid_save \
	hostname \
	inetd \
	initrandom \
	ip6addrctl \
	ipfilter \
	ipfs \
	ipfw \
	ipmon \
	ipnat \
	ipropd_master \
	ipropd_slave \
	ipsec \
	${_ipxrouted} \
	iscsictl \
	iscsid \
	jail \
	${_kadmind} \
	${_kdc} \
	keyserv \
	${_kfd} \
	kld \
	kldxref \
	${_kpasswdd} \
	ldconfig \
	local \
	localpkg \
	lockd \
	lpd \
	mixer \
	motd \
	mountcritlocal \
	mountcritremote \
	mountlate \
	mdconfig \
	mdconfig2 \
	mountd \
	moused \
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
	ntpd \
	ntpdate \
	${_opensm} \
	othermta \
	pf \
	pflog \
	pfsync \
	postrandom \
	powerd \
	power_profile \
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
	routed \
	routing \
	rpcbind \
	rtadvd \
	rtsold \
	rwho \
	savecore \
	sdpd \
	securelevel \
	sendmail \
	serial \
	sppp \
	${_sshd} \
	statd \
	static_arp \
	static_ndp \
	stf \
	swap \
	swaplate \
	syscons \
	sysctl \
	syslogd \
	timed \
	tmp \
	${_ubthidhci} \
	ugidfw \
	${_unbound} \
	${_utx} \
	var \
	virecover \
	watchdogd \
	wpa_supplicant \
	ypbind \
	yppasswdd \
	ypserv \
	ypset \
	ypupdated \
	ypxfrd \
	zfs \
	zvol

.if ${MK_IPX} != "no"
_ipxrouted=	ipxrouted
.endif

.if ${MK_KERBEROS} != "no"
_kadmind=	kadmind
_kdc=		kdc
_kfd=		kfd
_kpasswdd=	kpasswdd
.endif

.if ${MK_OFED} != "no"
_opensm=	opensm
.endif

.if ${MK_OPENSSH} != "no"
_sshd=		sshd
.endif

.if ${MK_NS_CACHING} != "no"
_nscd=		nscd
.endif

.if ${MK_BLUETOOTH} != "no"
_bluetooth=	bluetooth
_bthidd=	bthidd
_hcsecd=	hcsecd
_ubthidhci=	ubthidhci
.endif

.if ${MK_UNBOUND} != "no"
_unbound=	local_unbound
.endif

.if ${MK_UTMPX} != "no"
_utx=		utx
.endif

FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

.include <bsd.prog.mk>
