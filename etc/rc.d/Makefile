# $NetBSD: Makefile,v 1.16 2001/01/14 15:37:22 minoura Exp $
# $FreeBSD$

.include <bsd.own.mk>

FILES=	DAEMON LOGIN NETWORKING SERVERS abi accounting addswap adjkerntz altqd \
	amd apm apmd atm1 atm2.sh atm3.sh bgfsck bootconf.sh bootparams ccd \
	cleanvar cleartmp cron devdb devfs dhclient dhcpd dhcrelay diskless \
	dmesg dumpon fsck gated inetd initdiskless initrandom ipfilter ipfw \
	ip6fw ipmon ipnat ipsec ipxrouted isdnd kadmind kdc kerberos keyserv \
	ldconfig lkm1 lkm2 lkm3 local localdaemons lomac lpd mopd motd \
	mountall mountcritlocal mountcritremote mountd moused mroute6d \
	mrouted msgs named network network1 network2 network3 \
	network_ipv6 newsyslog nfsclient nfsd nfslocking \
	othermta nfsserver ntpd ntpdate pccard pcvt poffd ppp-user \
	postfix ppp pppoed pwcheck quota racoon raidframe random rarpd \
	rbootd root route6d routed rpcbind rtadvd rtsold \
	rwho savecore screenblank securelevel sendmail serial sppp sshd swap1 \
	swap2 sysdb syscons sysctl syslogd timed ttys usbd vinum virecover \
	wscons xdm xfs ypbind yppasswdd ypserv ypset ypupdated ypxfrd
FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

NOPROG= noprog

.include <bsd.prog.mk>
