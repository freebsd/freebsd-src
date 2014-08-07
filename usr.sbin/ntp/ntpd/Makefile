# $FreeBSD$

MAN=

.include <src.opts.mk>

NO_PIE=	yes

.PATH: ${.CURDIR}/../../../contrib/ntp/ntpd

PROG=	ntpd
SRCS=	cmd_args.c	ntp_config.c \
	ntp_control.c	ntp_crypto.c	ntp_filegen.c \
	ntp_intres.c	ntp_io.c	ntp_loopfilter.c \
	ntp_monitor.c	ntp_peer.c	ntp_proto.c \
	ntp_refclock.c	ntp_request.c \
	ntp_restrict.c	ntp_timer.c	ntp_util.c \
	ntpd.c	refclock_acts.c	refclock_arbiter.c \
	refclock_arc.c	refclock_as2201.c	refclock_atom.c \
	refclock_bancomm.c	refclock_chronolog.c	refclock_chu.c \
	refclock_conf.c	refclock_datum.c	refclock_dumbclock.c \
	refclock_fg.c	refclock_gpsvme.c	refclock_heath.c \
	refclock_hopfpci.c	refclock_hopfser.c	refclock_hpgps.c \
	refclock_irig.c	refclock_jupiter.c	refclock_leitch.c \
	refclock_local.c	refclock_msfees.c	refclock_mx4200.c \
	refclock_neoclock4x.c refclock_nmea.c	refclock_oncore.c \
	refclock_palisade.c refclock_parse.c	refclock_pcf.c \
	refclock_pst.c refclock_ripencc.c \
	refclock_shm.c	refclock_tpro.c refclock_trak.c	refclock_true.c \
	refclock_ulink.c refclock_wwv.c	\
	refclock_wwvb.c ntpd-opts.c \
	version.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../ \
	-I${.CURDIR}/../../../contrib/ntp/libopts -I${.CURDIR}

DPADD=	${LIBPARSE} ${LIBNTP} ${LIBM} ${LIBMD} ${LIBRT} ${LIBOPTS}
LDADD=	${LIBPARSE} ${LIBNTP} -lm -lmd -lrt ${LIBOPTS}

.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+=	-lcrypto
.endif

CLEANFILES+= .version version.c

version.c: 
	sh -e ${.CURDIR}/../scripts/mkver ntpd

.include <bsd.prog.mk>
