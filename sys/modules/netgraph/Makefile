# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/Makefile,v 1.8.2.2 2000/07/11 20:46:53 archie Exp $

SUBDIR=	async bpf cisco echo ether frame_relay hole iface ksocket lmi \
	netgraph ppp pppoe pptpgre rfc1490 socket tee tty UI vjc

.if !defined(NOCRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
SUBDIR+=	mppc
.endif

.include <bsd.subdir.mk>
