# $NetBSD: Makefile,v 1.16 2001/01/14 15:37:22 minoura Exp $
# $FreeBSD$

.include <bsd.own.mk>

FILES=	DAEMON LOGIN NETWORKING SERVERS abi accounting addswap adjkerntz amd \
	apm apmd archdep atm1 atm2 atm3 bgfsck bootparams ccd cleanvar \
	cleartmp cron devd devfs dhclient diskless dmesg dumpon early.sh fsck \
	gbde hostname inetd initdiskless initrandom ip6fw ipfilter ipfs ipfw ipmon \
	ipnat ipsec ipxrouted isdnd jail \
	kadmind kerberos keyserv kldxref kpasswdd \
	ldconfig local localdaemons lomac lpd \
	motd mountcritlocal mountcritremote \
	mountd moused mroute6d mrouted msgs \
	named netif netoptions network1 network2 network3 \
	network_ipv6 nfsclient nfsd nfslocking nfsserver nisdomain ntpd \
	ntpdate othermta pccard pcvt ppp-user pppoed pwcheck quota random \
	rarpd rcconf.sh root route6d routed routing rpcbind rtadvd rwho savecore \
	securelevel sendmail serial sppp sshd swap1 syscons sysctl \
	syslogd timed ttys usbd vinum virecover watchdogd ypbind \
	yppasswdd ypserv ypset ypupdated ypxfrd
FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

NOPROG= noprog

.include <bsd.prog.mk>
