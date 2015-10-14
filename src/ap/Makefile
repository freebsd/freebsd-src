all: libap.a

clean:
	rm -f *~ *.o *.d *.gcno *.gcda *.gcov libap.a

install:
	@echo Nothing to be made.

include ../lib.rules

CFLAGS += -DHOSTAPD
CFLAGS += -DNEED_AP_MLME
CFLAGS += -DCONFIG_HS20
CFLAGS += -DCONFIG_INTERWORKING
CFLAGS += -DCONFIG_IEEE80211R
CFLAGS += -DCONFIG_IEEE80211W
CFLAGS += -DCONFIG_WPS
CFLAGS += -DCONFIG_PROXYARP
CFLAGS += -DCONFIG_IAPP

LIB_OBJS= \
	accounting.o \
	ap_config.o \
	ap_drv_ops.o \
	ap_list.o \
	ap_mlme.o \
	authsrv.o \
	beacon.o \
	bss_load.o \
	ctrl_iface_ap.o \
	dfs.o \
	dhcp_snoop.o \
	drv_callbacks.o \
	eap_user_db.o \
	gas_serv.o \
	hostapd.o \
	hs20.o \
	hw_features.o \
	iapp.o \
	ieee802_11_auth.o \
	ieee802_11.o \
	ieee802_11_ht.o \
	ieee802_11_shared.o \
	ieee802_11_vht.o \
	ieee802_1x.o \
	ndisc_snoop.o \
	p2p_hostapd.o \
	peerkey_auth.o \
	pmksa_cache_auth.o \
	preauth_auth.o \
	sta_info.o \
	tkip_countermeasures.o \
	utils.o \
	vlan_init.o \
	wmm.o \
	wnm_ap.o \
	wpa_auth.o \
	wpa_auth_ft.o \
	wpa_auth_glue.o \
	wpa_auth_ie.o \
	wps_hostapd.o \
	x_snoop.o

libap.a: $(LIB_OBJS)
	$(AR) crT $@ $?

-include $(OBJS:%.o=%.d)
