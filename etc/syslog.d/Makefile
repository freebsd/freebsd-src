# $FreeBSD$

.include <src.opts.mk>

.if ${MK_FTP} != "no"
FILES+=		ftp.conf
.endif

.if ${MK_LPR} != "no"
FILES+=		lpr.conf
.endif

.if ${MK_PPP} != "no"
FILES+=		ppp.conf
.endif

BINDIR=		/etc/syslog.d

.include <bsd.prog.mk>
