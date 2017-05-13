# $FreeBSD$

.include <src.opts.mk>

BINDIR=		/etc/newsyslog.conf.d

FILES=

.if ${MK_AMD} != "no"
FILES+=		amd.conf
.endif

.if ${MK_FTP} != "no"
FILES+=		ftp.conf
.endif

.if ${MK_LPR} != "no"
FILES+=		lpr.conf
.endif

.if ${MK_OFED} != "no"
FILES+=		opensm.conf
.endif

.if ${MK_PF} != "no"
FILES+=		pf.conf
.endif

.if ${MK_PPP} != "no"
FILES+=		ppp.conf
.endif

.if ${MK_SENDMAIL} != "no"
FILES+=		sendmail.conf
.endif

.include <bsd.prog.mk>
