# $FreeBSD$

DEVIWLWIFIDIR=	${SRCTOP}/sys/contrib/dev/iwlwifi

.PATH: ${DEVIWLWIFIDIR}

KMOD=	if_iwlwifi

SRCS=	iwl-drv.c
SRCS+=	iwl-dbg-tlv.c iwl-debug.c
SRCS+=	iwl-eeprom-parse.c iwl-eeprom-read.c
SRCS+=	iwl-io.c iwl-nvm-parse.c iwl-phy-db.c iwl-trans.c
SRCS+=	cfg/7000.c cfg/8000.c cfg/9000.c cfg/22000.c
SRCS+=	fw/dbg.c fw/dump.c fw/img.c fw/notif-wait.c
SRCS+=	fw/paging.c fw/pnvm.c fw/rs.c fw/smem.c fw/init.c
#SRCS+=	fw/uefi.c
SRCS+=	mvm/rs.c mvm/binding.c mvm/coex.c mvm/ftm-initiator.c
SRCS+=	mvm/ftm-responder.c mvm/fw.c mvm/mac-ctxt.c
SRCS+=	mvm/mac80211.c mvm/nvm.c mvm/offloading.c mvm/ops.c
SRCS+=	mvm/phy-ctxt.c mvm/power.c mvm/quota.c mvm/rs-fw.c mvm/rfi.c
SRCS+=	mvm/rx.c mvm/rxmq.c mvm/scan.c mvm/sf.c mvm/sta.c mvm/tdls.c
SRCS+=	mvm/time-event.c mvm/tt.c mvm/tx.c mvm/utils.c
#SRCS+=	mvm/led.c
SRCS+=	pcie/ctxt-info-gen3.c pcie/ctxt-info.c
SRCS+=	pcie/drv.c pcie/rx.c pcie/trans-gen2.c pcie/trans.c
SRCS+=	pcie/tx-gen2.c pcie/tx.c
SRCS+=	queue/tx.c

SRCS+=	iwl-devtrace.c

# Other
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_wlan.h opt_inet6.h opt_inet.h opt_acpi.h

CFLAGS+=	-DKBUILD_MODNAME='"iwlwifi"'

CFLAGS+=	-I${DEVIWLWIFIDIR}
CFLAGS+=	-I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+=	-DCONFIG_IWLDVM=0
CFLAGS+=	-DCONFIG_IWLMVM=1
# Helpful after fresh imports.
#CFLAGS+=	-ferror-limit=0

#CFLAGS+=	-DCONFIG_ACPI=1
#CFLAGS+=	-DCONFIG_INET=1		# Need LKPI TSO implementation.
#CFLAGS+=	-DCONFIG_IPV6=1
#CFLAGS+=	-DCONFIG_IWLWIFI_BCAST_FILTERING=1
CFLAGS+=	-DCONFIG_IWLWIFI_DEBUG=1
#CFLAGS+=	-DCONFIG_IWLWIFI_LEDS=1
CFLAGS+=	-DCONFIG_IWLWIFI_OPMODE_MODULAR=1
CFLAGS+=	-DCONFIG_IWLWIFI_DEVICE_TRACING=1
#CFLAGS+=	-DCONFIG_LOCKDEP=1
#CFLAGS+=	-DCONFIG_MAC80211_DEBUGFS=1
#CFLAGS+=	-DCONFIG_NL80211_TESTMODE=1
#CFLAGS+=	-DCONFIG_PM=1
#CFLAGS+=	-DCONFIG_PM_SLEEP=1
#CFLAGS+=	-DCONFIG_THERMAL=1
#CFLAGS+=	-DCONFIG_EFI=1

.include <bsd.kmod.mk>
