# $FreeBSD$

.include <src.opts.mk>

BINDIR=		/etc/syslog.d

FILES=

.if ${MK_BSNMP} != "no"
FILES+=		bsnmpd.conf
.endif

.include <bsd.prog.mk>
