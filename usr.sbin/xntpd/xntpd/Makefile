#
# $Id: Makefile,v 1.1 1993/12/21 20:16:18 wollman Exp $
#

CFLAGS+= -I${.CURDIR}/../include
LDADD+= -L${.CURDIR}/../lib -lntp -L${.CURDIR}/../parse -lparse
DPADD+= ${.CURDIR}/../lib/libntp.a ${.CURDIR}/../parse/libparse.a

PROG= xntpd
MAN8= ${.CURDIR}/../doc/xntpd.8
CLEANFILES+= version.c

SRCS=	ntp_config.c ntp_control.c ntp_io.c ntp_leap.c \
	ntp_loopfilter.c ntp_monitor.c ntp_peer.c ntp_proto.c \
	ntp_refclock.c ntp_request.c ntp_restrict.c ntp_timer.c \
	ntp_unixclock.c ntp_util.c ntpd.c refclock_chu.c \
	refclock_conf.c refclock_local.c refclock_pst.c \
	refclock_wwvb.c refclock_goes.c refclock_mx4200.c \
	refclock_parse.c refclock_as2201.c refclock_omega.c \
	refclock_tpro.c refclock_leitch.c refclock_irig.c \
	refclock_msfees.c ntp_intres.c ntp_filegen.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion xntpd

.include <bsd.prog.mk>
