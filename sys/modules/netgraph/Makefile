# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/Makefile,v 1.8.2.10 2004/03/10 15:35:19 ru Exp $

SUBDIR=	UI \
	async \
	bpf \
	bridge \
	cisco \
	echo \
	eiface \
	etf \
	ether \
	frame_relay \
	hole \
	iface \
	ksocket \
	l2tp \
	lmi \
	netgraph \
	one2many \
	ppp \
	pppoe \
	pptpgre \
	rfc1490 \
	socket \
	sync_ar \
	sync_sr \
	tee \
	tty \
	vjc \
	vlan

.if !defined(NOCRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
SUBDIR+=	mppc
.endif

.include <bsd.subdir.mk>
