#
# $FreeBSD: src/usr.sbin/ntp/ntpd/Makefile,v 1.3 2000/01/01 23:57:59 roberto Exp $
#

.PATH:	${.CURDIR}/../../../contrib/ntp/ntpd

PROG= ntpd

SRCS=	map_vme.c	ntp_config.c \
	ntp_control.c	ntp_filegen.c	ntp_intres.c \
	ntp_io.c	ntp_loopfilter.c	ntp_monitor.c \
	ntp_peer.c	ntp_proto.c	ntp_refclock.c \
	ntp_request.c	ntp_restrict.c	ntp_timer.c \
	ntp_util.c	ntpd.c	refclock_acts.c \
	refclock_arbiter.c	refclock_arc.c	refclock_as2201.c \
	refclock_atom.c	refclock_bancomm.c	refclock_chronolog.c \
	refclock_chu.c	refclock_conf.c	refclock_datum.c \
	refclock_dumbclock.c	refclock_gpsvme.c	refclock_heath.c \
	refclock_hpgps.c	refclock_irig.c	refclock_jupiter.c \
	refclock_leitch.c	refclock_local.c	refclock_msfees.c \
	refclock_mx4200.c	refclock_nmea.c	refclock_oncore.c \
	refclock_palisade.c	refclock_parse.c	refclock_pst.c \
	refclock_ptbacts.c	refclock_shm.c	refclock_tpro.c \
	refclock_trak.c	refclock_true.c	refclock_usno.c \
	refclock_wwvb.c	 version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN= yes

DPADD=	${LIBPARSE} ${LIBNTP} -lm
LDADD=	${LIBPARSE} ${LIBNTP} -lm

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpd

afterinstall:
	rm -f ${DESTDIR}/usr/sbin/xntpd

.include <bsd.prog.mk>
