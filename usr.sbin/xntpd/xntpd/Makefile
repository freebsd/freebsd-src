#
# $FreeBSD$
#

CFLAGS+= -I${.CURDIR}/../include

.if exists(${.OBJDIR}/../lib)
LDADD+= -L${.OBJDIR}/../lib
DPADD+= ${.OBJDIR}/../lib/libntp.a
.else
LDADD+= -L${.CURDIR}/../lib
DPADD+= ${.CURDIR}/../lib/libntp.a
.endif

.if exists(${.OBJDIR}/../parse)
LDADD+= -L${.OBJDIR}/../parse
DPADD+= ${.OBJDIR}/../parse/libparse.a
.else
LDADD+= -L${.CURDIR}/../parse
DPADD+= ${.CURDIR}/../parse/libparse.a
.endif

LDADD+= -lntp -lparse -lkvm
DPADD+= ${LIBKVM}

PROG= xntpd
MAN8= ${.CURDIR}/../doc/xntpd.8
CLEANFILES+= .version version.c

SRCS=	ntp_config.c ntp_control.c ntp_io.c ntp_leap.c \
	ntp_loopfilter.c ntp_monitor.c ntp_peer.c ntp_proto.c \
	ntp_refclock.c ntp_request.c ntp_restrict.c ntp_timer.c \
	ntp_unixclock.c ntp_util.c ntpd.c ntp_intres.c \
	ntp_filegen.c version.c

# refclocks
SRCS+=	refclock_acts.c refclock_as2201.c refclock_atom.c refclock_chu.c \
	refclock_conf.c refclock_datum.c refclock_goes.c refclock_gpstm.c \
	refclock_heath.c refclock_leitch.c refclock_local.c refclock_moto.c \
	refclock_msfees.c refclock_mx4200.c refclock_nmea.c refclock_omega.c \
	refclock_parse.c refclock_pst.c refclock_trak.c refclock_wwvb.c

beforedepend: version.c

version.c: ${.CURDIR}/../VERSION
	sh -e ${.CURDIR}/../scripts/mkversion xntpd

.include <bsd.prog.mk>
