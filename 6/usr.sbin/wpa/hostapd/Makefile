# $FreeBSD$

HOSTAPD_DISTDIR?= ${.CURDIR}/../../../contrib/hostapd
.PATH: ${.CURDIR}/.. ${HOSTAPD_DISTDIR}

PROG=	hostapd
SRCS=	hostapd.c eloop.c ieee802_1x.c eapol_sm.c radius.c md5.c rc4.c \
	common.c ieee802_11.c config.c ieee802_11_auth.c accounting.c \
	sta_info.c radius_client.c sha1.c wpa.c aes_wrap.c tls_none.c \
	ctrl_iface.c driver_conf.c l2_packet.c driver_freebsd.c
CLEANFILES=driver_conf.c

MAN=	hostapd.8 hostapd.conf.5

CFLAGS+= -I${.CURDIR} -I${HOSTAPD_DISTDIR}
CFLAGS+= -DCONFIG_DRIVER_BSD
DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

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
