# $NetBSD: Makefile,v 1.16 2001/01/14 15:37:22 minoura Exp $
# $FreeBSD$

FILES=	DAEMON LOGIN NETWORKING SERVERS \
	abi accounting addswap adjkerntz amd \
	apm apmd archdep atm1 atm2 atm3 auditd auto_linklocal \
	bgfsck bluetooth bootparams bridge bsnmpd bthidd \
	ccd cleanvar cleartmp cron \
	devd devfs dhclient \
	dmesg dumpon \
	early.sh encswap \
	fsck ftpd \
	gbde geli geli2 \
	hcsecd \
	hostapd hostname \
	ike \
	inetd initrandom \
	ip6addrctl ip6fw ipfilter ipfs ipfw ipmon \
	ipnat ipsec ipxrouted isdnd \
	jail \
	kadmind kerberos keyserv kldxref kpasswdd \
	ldconfig local localpkg lpd \
	mixer motd mountcritlocal mountcritremote mountlate \
	mdconfig mdconfig2 mountd moused mroute6d mrouted msgs \
	named natd netif netoptions \
	network_ipv6 newsyslog nfsclient nfsd \
	nfslocking nfsserver nisdomain nsswitch ntpd ntpdate \
	othermta \
	pccard pcvt pf pflog pfsync \
	powerd power_profile ppp pppoed pwcheck \
	quota \
	ramdisk ramdisk-own random rarpd resolv root \
	route6d routed routing rpcbind rtadvd rwho \
	savecore sdpd securelevel sendmail \
	serial sppp swap1 \
	syscons sysctl syslogd \
	timed tmp \
	ugidfw usbd \
	var virecover \
	watchdogd wpa_supplicant \
	ypbind yppasswdd ypserv \
	ypset ypupdated ypxfrd

.if !defined(NO_OPENSSH)
FILES+=	sshd
.endif

FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

.include <bsd.prog.mk>
