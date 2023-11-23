
.include <src.opts.mk>

.PATH: ${SRCTOP}/usr.bin/wall

PACKAGE=	syslogd

CONFGROUPS=	CONFS SYSLOGD_D
CONFS=	syslog.conf
PROG=	syslogd
MAN=	syslog.conf.5 syslogd.8
SRCS=	syslogd.c ttymsg.c
SYSLOGD_DPACAKGE=	syslogd

LIBADD=	util

.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

SYSLOGD_D=
SYSLOGD_DDIR=	/etc/syslog.d
.if ${MK_FTP} != "no"
CONFGROUPS+=	FTP
FTP+=		ftp.conf
FTPDIR=		/etc/syslog.d
FTPPACKAGE=	ftpd
.endif

.if ${MK_LPR} != "no"
SYSLOGD_D+=	lpr.conf
.endif

.if ${MK_PPP} != "no"
CONFGROUPS+=	PPP
PPP+=		ppp.conf
PPPDIR=		/etc/syslog.d
PPPPACKAGE=	ppp
.endif

CFLAGS+= -I${SRCTOP}/usr.bin/wall

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
