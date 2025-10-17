DEVIWLWIFIDIR=	${SRCTOP}/sys/contrib/dev/iwlwifi

.PATH: ${DEVIWLWIFIDIR}

WITH_CONFIG_PM=	0
WITH_DEBUGFS=	0
WITH_CONFIG_ACPI=	1

KMOD=	if_iwlwifi

SRCS=	iwl-dbg-tlv.c iwl-drv.c iwl-debug.c iwl-devtrace.c iwl-io.c
SRCS+=	iwl-nvm-parse.c iwl-nvm-utils.c iwl-trans.c iwl-phy-db.c
SRCS+=	iwl-utils.c

SRCS+=	cfg/22000.c cfg/7000.c cfg/8000.c cfg/9000.c cfg/ax210.c
SRCS+=	cfg/bz.c cfg/sc.c cfg/dr.c
SRCS+=	cfg/rf-fm.c cfg/rf-gf.c cfg/rf-hr.c cfg/rf-jf.c cfg/rf-pe.c
SRCS+=	cfg/rf-wh.c

SRCS+=	fw/dbg.c fw/dump.c fw/img.c fw/init.c
SRCS+=	fw/notif-wait.c fw/paging.c fw/pnvm.c fw/regulatory.c fw/rs.c
SRCS+=	fw/smem.c
#SRCS+=	fw/uefi.c

SRCS+=	pcie/ctxt-info-v2.c pcie/ctxt-info.c pcie/drv.c pcie/utils.c
SRCS+=	pcie/gen1_2/rx.c pcie/gen1_2/trans-gen2.c pcie/gen1_2/trans.c
SRCS+=	pcie/gen1_2/tx-gen2.c pcie/gen1_2/tx.c

SRCS+=	mvm/binding.c mvm/coex.c
SRCS+=	mvm/ftm-initiator.c mvm/ftm-responder.c mvm/fw.c
#SRCS+=	mvm/led.c
SRCS+=	mvm/link.c mvm/mac-ctxt.c mvm/mac80211.c mvm/mld-key.c
SRCS+=	mvm/mld-mac.c mvm/mld-mac80211.c mvm/mld-sta.c mvm/nvm.c
SRCS+=	mvm/offloading.c mvm/ops.c mvm/phy-ctxt.c mvm/power.c mvm/ptp.c
SRCS+=	mvm/quota.c mvm/rfi.c mvm/rs-fw.c mvm/rs.c mvm/rx.c mvm/rxmq.c
SRCS+=	mvm/scan.c mvm/sf.c mvm/sta.c mvm/tdls.c mvm/time-event.c
SRCS+=	mvm/time-sync.c mvm/tt.c mvm/tx.c mvm/utils.c

SRCS+=	mld/agg.c mld/ap.c mld/coex.c
SRCS+=	mld/ftm-initiator.c mld/fw.c mld/iface.c mld/key.c
#SRCS+=	mld/led.c
SRCS+=	mld/link.c mld/low_latency.c mld/mac80211.c mld/mcc.c mld/mld.c
SRCS+=	mld/mlo.c mld/notif.c mld/phy.c mld/power.c mld/ptp.c
SRCS+=	mld/regulatory.c mld/roc.c mld/rx.c mld/scan.c
SRCS+=	mld/session-protect.c mld/sta.c mld/stats.c mld/thermal.c
SRCS+=	mld/time_sync.c mld/tlc.c mld/tx.c

.if defined(WITH_DEBUGFS) && ${WITH_DEBUGFS} > 0
SRCS+=	fw/debugfs.c
SRCS+=	mvm/debugfs.c mvm/debugfs-vif.c
SRCS+=	mld/debugfs.c
CFLAGS+=	-DCONFIG_IWLWIFI_DEBUGFS
CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS
.endif
.if defined(WITH_CONFIG_PM) && ${WITH_CONFIG_PM} > 0
SRCS+=	mvm/d3.c
SRCS+=	mld/d3.c
CFLAGS+=	-DCONFIG_PM
CFLAGS+=	-DCONFIG_PM_SLEEP
.endif

.if defined(WITH_CONFIG_ACPI) && ${WITH_CONFIG_ACPI} > 0
SRCS+=	fw/acpi.c
CFLAGS+=	-DCONFIG_ACPI
CFLAGS+=	-DLINUXKPI_WANT_LINUX_ACPI
.endif

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"iwlwifi"'
CFLAGS+=	-DLINUXKPI_VERSION=61700

CFLAGS+=	-I${DEVIWLWIFIDIR}
CFLAGS+=	${LINUXKPI_INCLUDES}
CFLAGS+=	-DCONFIG_IWLDVM=0
CFLAGS+=	-DCONFIG_IWLMVM=1
CFLAGS+=	-DCONFIG_IWLMLD=1
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

#CFLAGS+=	-DCONFIG_INET=1		# Need LKPI TSO implementation.
#CFLAGS+=	-DCONFIG_IPV6=1
CFLAGS+=	-DCONFIG_IWLWIFI_DEBUG=1
#CFLAGS+=	-DCONFIG_IWLWIFI_LEDS=1
#CFLAGS+=	-DCONFIG_IWLWIFI_OPMODE_MODULAR=1
CFLAGS+=	-DCONFIG_IWLWIFI_DEVICE_TRACING=1
#CFLAGS+=	-DCONFIG_LOCKDEP=1
#CFLAGS+=	-DCONFIG_NL80211_TESTMODE=1
#CFLAGS+=	-DCONFIG_THERMAL=1
#CFLAGS+=	-DCONFIG_EFI=1

# XXX-BZ how to do this just for pcie/drv.c (and gcc vs. clang)?
CFLAGS += -Wno-override-init -Wno-initializer-overrides

.include <bsd.kmod.mk>
