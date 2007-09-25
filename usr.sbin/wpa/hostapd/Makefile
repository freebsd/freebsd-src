# $FreeBSD$

.include <bsd.own.mk>

HOSTAPD_DISTDIR?= ${.CURDIR}/../../../contrib/hostapd
.PATH: ${.CURDIR}/.. ${HOSTAPD_DISTDIR}

PROG=	hostapd
SRCS=	hostapd.c eloop.c ieee802_1x.c eapol_sm.c radius.c md5.c rc4.c \
	common.c ieee802_11.c config.c ieee802_11_auth.c accounting.c \
	sta_info.c radius_client.c sha1.c wpa.c aes_wrap.c ctrl_iface.c \
	driver_conf.c os_unix.c preauth.c pmksa_cache.c beacon.c \
	hw_features.c wme.c ap_list.c reconfig.c mlme.c \
	vlan_init.c ieee802_11h.c l2_packet.c driver_freebsd.c
CLEANFILES=driver_conf.c

MAN=	hostapd.8 hostapd.conf.5

CFLAGS+= -I${.CURDIR} -I${HOSTAPD_DISTDIR}
CFLAGS+= -DCONFIG_DRIVER_BSD
CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX
.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif
CFLAGS+= -g
DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

# User customizations for wpa_supplicant/hostapd build environment
CFLAGS+=${WPA_SUPPLICANT_CFLAGS}
#DPADD+=${WPA_SUPPLICANT_DPADD}
LDADD+=${WPA_SUPPLICANT_LDADD}
#LDFLAGS+=${WPA_SUPPLICANT_LDFLAGS}

.if !empty(CFLAGS:M*-DEAP_SERVER)
SRCS+=	eap.c eap_methods.c eap_identity.c

.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)

CFLAGS+=-DEAP_TLS -DEAP_PEAP -DEAP_MSCHAPv2 -DEAP_PSK \
	-DEAP_TLV -DEAP_TLS_FUNCS -DEAP_TLS_OPENSSL
SRCS+=	eap_tls.c eap_peap.c eap_mschapv2.c \
	eap_psk.c eap_psk_common.c \
	eap_tlv.c eap_tls_common.c tls_openssl.c ms_funcs.c crypto.c

CFLAGS+=-DEAP_TTLS -DEAP_MD5
SRCS+=	eap_ttls.c eap_md5.c

.if !empty(CFLAGS:M*-DEAP_GTC)
SRCS+=	eap_gtc.c
.endif

.if !empty(CFLAGS:M*-DEAP_AKA)
NEED_SIM_COMMON=	true
SRCS+=	eap_aka.c
.endif

.if !empty(CFLAGS:M*-DEAP_SIM)
NEED_SIM_COMMON=	true
SRCS+=	eap_sim.c
.endif

.if defined(NEED_SIM_COMMON)
SRCS+=	eap_sim_common.c eap_sim_db.c
.endif

.if !empty(CFLAGS:M*-DEAP_GPSK)
CFLAGS+=-DEAP_GPSK_SHA256
SRCS+=	eap_gpsk.c eap_gpsk_common.c
NEED_SHA256=	true
.endif

.if !empty(CFLAGS:M*-DEAP_PAX)
SRCS+=	eap_pax.c eap_pax_common.c
.endif

.if !empty(CFLAGS:M*-DEAP_SAKE)
SRCS+=	eap_sake.c eap_sake_common.c
.endif

DPADD+= ${LIBSSL} ${LIBCRYPTO}
LDADD+= -lssl -lcrypto
.else
NEED_TLS_NONE=	true
.endif

.else
NEED_TLS_NONE=	true
.endif

.if defined(NEED_SHA256)
CFLAGS+=-DINTERNAL_SHA256
SRCS+=	sha256.c
.endif

.if defined(NEED_TLS_NONE)
CFLAGS+= -DEAP_TLS_NONE
CFLAGS+= -DINTERNAL_AES
CFLAGS+= -DINTERNAL_SHA1
CFLAGS+= -DINTERNAL_MD5
SRCS+=	tls_none.c
.endif

driver_conf.c: Makefile
	rm -f driver_conf.c
	echo '/* THIS FILE AUTOMATICALLY GENERATED, DO NOT EDIT! */' \
							> driver_conf.c
	echo '#include <stdlib.h>'			>> driver_conf.c
	echo '#include <stdio.h>'			>> driver_conf.c
	echo '#include <sys/types.h>'			>> driver_conf.c
	echo '#include <netinet/in.h>'			>> driver_conf.c
	echo '#include "hostapd.h"'			>> driver_conf.c
	echo '#include "driver.h"'			>> driver_conf.c
	echo "void bsd_driver_register(void);"		>> driver_conf.c
	echo 'void register_drivers(void) {'		>> driver_conf.c
	echo "bsd_driver_register();"			>> driver_conf.c
	echo '}'					>> driver_conf.c

.include <bsd.prog.mk>
