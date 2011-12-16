# $FreeBSD$

.include <bsd.own.mk>

FILES=	DAEMON FILESYSTEMS LOGIN NETWORKING SERVERS \
	abi accounting addswap adjkerntz amd \
	apm apmd archdep atm1 atm2 atm3 auditd \
	bgfsck bluetooth bootparams bridge bsnmpd bthidd \
	ccd cleanvar cleartmp cron \
	ddb defaultroute devd devfs dhclient \
	dmesg dumpon \
	encswap \
	faith fsck ftp-proxy ftpd \
	gbde geli geli2 gptboot gssd \
	hastd hcsecd \
	hostapd hostid hostid_save hostname \
	inetd initrandom \
	ip6addrctl ipfilter ipfs ipfw ipmon \
	ipnat ipsec \
	jail \
	kadmind kerberos keyserv kld kldxref kpasswdd \
	ldconfig local localpkg lockd lpd \
	mixer motd mountcritlocal mountcritremote mountlate \
	mdconfig mdconfig2 mountd moused mroute6d mrouted msgs \
	named natd netif netoptions netwait \
	newsyslog nfsclient nfscbd nfsd \
	nfsuserd nisdomain nsswitch ntpd ntpdate \
	othermta \
	pf pflog pfsync \
	powerd power_profile ppp pppoed pwcheck \
	quota \
	random rarpd rctl resolv rfcomm_pppd_server root \
	route6d routed routing rpcbind rtadvd rtsold rwho \
	savecore sdpd securelevel sendmail \
	serial sppp statd static_arp static_ndp stf swap1 \
	syscons sysctl syslogd \
	timed tmp \
	ugidfw \
	var virecover \
	watchdogd wpa_supplicant \
	ypbind yppasswdd ypserv \
	ypset ypupdated ypxfrd \
	zfs zvol

.if ${MK_IPX} != "no"
FILES+=	ipxrouted
.endif

.if ${MK_OFED} != "no"
FILES+= opensm
.endif

.if ${MK_OPENSSH} != "no"
FILES+=	sshd
.endif

.if ${MK_NS_CACHING} != "no"
FILES+= nscd
.endif

.if ${MK_BLUETOOTH} != "no"
FILES+=	ubthidhci
.endif

FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

.include <bsd.prog.mk>
