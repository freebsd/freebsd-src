# $FreeBSD$

.include <src.opts.mk>

BINDIR=		/etc
FILESGROUPS=	RCETC RCETCEXEC RCETCDEFAULTS
RCETC=	network.subr rc rc.initdiskless rc.subr rc.shutdown rc.bsdextended
RCETCPACKAGE=	rc

.if ${MK_IPFW} != "no"
FILESGROUPS+=	RCIPFW
RCIPFW+=	rc.firewall
RCIPFWPACKAGE=	ipfw
.endif
.if ${MK_SENDMAIL} != "no"
FILESGROUPS+=	RCSENDMAIL
RCSENDMAIL+=	rc.sendmail
RCSENDMAILPACKAGE=	sendmail
.endif
RCETCMODE=	644
RCETCEXEC=	netstart pccard_ether rc.resume rc.suspend
RCETCEXECMODE=	755
RCETCEXECPACKAGE=	rc
RCETCDEFAULTSDIR=	/etc/defaults
RCETCDEFAULTS=		rc.conf
RCETCDEFAULTSPACKAGE=	rc

SUBDIR+=	rc.d

.include <bsd.prog.mk>
