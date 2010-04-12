# $FreeBSD$

.PATH: ${.CURDIR}/../../net80211

KMOD=	wlan
SRCS=	ieee80211.c ieee80211_action.c ieee80211_ageq.c ieee80211_amrr.c \
	ieee80211_crypto.c ieee80211_crypto_none.c ieee80211_dfs.c \
	ieee80211_freebsd.c ieee80211_input.c ieee80211_ioctl.c \
	ieee80211_mesh.c ieee80211_node.c ieee80211_output.c ieee80211_phy.c \
	ieee80211_power.c ieee80211_proto.c ieee80211_scan.c \
	ieee80211_scan_sta.c ieee80211_radiotap.c ieee80211_ratectl.c \
	ieee80211_regdomain.c ieee80211_rssadapt.c \
	ieee80211_ht.c ieee80211_hwmp.c ieee80211_adhoc.c ieee80211_hostap.c \
	ieee80211_monitor.c ieee80211_sta.c ieee80211_wds.c ieee80211_ddb.c
SRCS+=	bus_if.h device_if.h opt_inet.h opt_inet6.h opt_ipx.h opt_wlan.h \
	opt_ddb.h

.if !defined(KERNBUILDDIR)
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
	echo "#define IEEE80211_AMDPU_AGE 1" >> ${.TARGET}
	echo "#define IEEE80211_SUPPORT_MESH 1" >> ${.TARGET}
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
# override to get ddb support?
opt_ddb.h:
	:> ${.TARGET}
.endif

.include <bsd.kmod.mk>
