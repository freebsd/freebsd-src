#
# $Id: Makefile,v 1.3 1993/12/23 10:21:55 rgrimes Exp $
#

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.CURDIR}/../lib/obj)
LDADD+= -L${.CURDIR}/../lib/obj
DPADD+= ${.CURDIR}/../lib/obj/libntp.a
.else
LDADD+= -L${.CURDIR}/../lib
DPADD+= ${.CURDIR}/../lib/libntp.a
.endif

.if exists(${.CURDIR}/../parse/obj)
LDADD+= -L${.CURDIR}/../parse/obj
DPADD+= ${.CURDIR}/../parse/obj/libparse.a
.else
LDADD+= -L${.CURDIR}/../parse
DPADD+= ${.CURDIR}/../parse/libparse.a
.endif

LDADD+= -lntp -lparse

PROG= xntpd
MAN8= ${.CURDIR}/../doc/xntpd.8
CLEANFILES+= .version version.c

SRCS=	ntp_config.c ntp_control.c ntp_io.c ntp_leap.c \
	ntp_loopfilter.c ntp_monitor.c ntp_peer.c ntp_proto.c \
	ntp_refclock.c ntp_request.c ntp_restrict.c ntp_timer.c \
	ntp_unixclock.c ntp_util.c ntpd.c refclock_chu.c \
	refclock_conf.c refclock_local.c refclock_pst.c \
	refclock_wwvb.c refclock_goes.c refclock_mx4200.c \
	refclock_parse.c refclock_as2201.c refclock_omega.c \
	refclock_tpro.c refclock_leitch.c refclock_irig.c \
	refclock_msfees.c refclock_gpstm.c ntp_intres.c \
	ntp_filegen.c version.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	${.CURDIR}/../scripts/mkversion xntpd

.include <bsd.prog.mk>
