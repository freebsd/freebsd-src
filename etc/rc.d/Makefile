# $NetBSD: Makefile,v 1.16 2001/01/14 15:37:22 minoura Exp $
# $FreeBSD$

.include <bsd.own.mk>

FILES=	DAEMON LOGIN NETWORKING SERVERS abi accounting addswap adjkerntz amd \
	apm apmd atm1 atm2.sh atm3.sh bgfsck ccd cleanvar cleartmp cron devdb \
	devfs diskless dmesg dumpon fsck inetd initdiskless initrandom ip6fw \
	ipfilter ipfw ipmon ipnat ipsec ipxrouted isdnd kadmind kerberos \
	keyserv ldconfig local localdaemons lomac lpd motd mountcritlocal \
	mountcritremote mountd moused mroute6d mrouted msgs named network1 \
	network2 network3 network_ipv6 nfsclient nfsd nfslocking nfsserver \
	ntpd ntpdate othermta pccard pcvt ppp-user pppoed pwcheck quota \
	random rarpd root route6d routed rpcbind rtadvd rwho savecore \
	securelevel sendmail serial sppp sshd swap1 syscons sysctl syslogd \
	timed ttys usbd vinum virecover ypbind yppasswdd ypserv ypset \
	ypupdated ypxfrd
FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

NOPROG= noprog

.include <bsd.prog.mk>
