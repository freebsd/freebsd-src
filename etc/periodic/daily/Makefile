# $FreeBSD: src/etc/periodic/daily/Makefile,v 1.4.2.1 2000/10/31 09:44:08 dougb Exp $

BIN=    100.clean-disks \
	110.clean-tmps \
	120.clean-preserve \
	130.clean-msgs \
	140.clean-rwho \
	150.clean-hoststat \
	200.backup-passwd \
	210.backup-aliases \
	220.backup-distfile \
	300.calendar \
	310.accounting \
	320.rdist \
	330.news \
	400.status-disks \
	420.status-network \
	430.status-rwho \
	440.status-mailq \
	450.status-security \
	460.status-mail-rejects \
	999.local
.if !defined(NOUUCP)
BIN+=	340.uucp \
	410.status-uucp
.endif

.include <bsd.prog.mk>
