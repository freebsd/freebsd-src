# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/Makefile,v 1.8.2.6 2002/04/09 21:30:13 julian Exp $

SUBDIR=	async \
	bpf \
	bridge \
	cisco \
	echo \
	ether \
	etf \
	frame_relay \
	hole \
	iface \
	ksocket \
	lmi \
	netgraph \
	one2many \
	ppp \
	pppoe \
	pptpgre \
	rfc1490 \
	socket \
	tee \
	tty \
	UI \
	vjc

.if !defined(NOCRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
SUBDIR+=	mppc
.endif

.include <bsd.subdir.mk>
