#
# $FreeBSD$
#

MAINTAINER=markm@FreeBSD.org

LIB=	wrap
SHLIB_MAJOR= 3
SHLIB_MINOR= 0
INCS=	tcpd.h
MAN=	hosts_access.3
MAN+=	hosts_access.5 hosts_options.5

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers

CFLAGS+=-DFACILITY=LOG_AUTH -DHOSTS_ACCESS -DNETGROUP -DDAEMON_UMASK=022 \
	-DREAL_DAEMON_DIR=\"/usr/libexec\" -DPROCESS_OPTIONS \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\" \
	-DSYS_ERRLIST_DEFINED -DALWAYS_HOSTNAME
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

SRCS=	clean_exit.c diag.c eval.c fix_options.c fromhost.c \
	hosts_access.c hosts_ctl.c misc.c myvsyslog.c options.c \
	percent_m.c percent_x.c refuse.c rfc931.c shell_cmd.c \
	socket.c tli.c update.c workarounds.c libvars.c

.include <bsd.lib.mk>
