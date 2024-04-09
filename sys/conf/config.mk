#
# Common code to marry kernel config(8) goo and module building goo.
#

# Generate options files that otherwise would be built
# in substantially similar ways through the tree. Move
# the code here when they all produce identical results
# (or should)
.if !defined(KERNBUILDDIR)
opt_global.h:
	touch ${.TARGET}
	@echo "#define SMP 1" >> ${.TARGET}
	@echo "#define MAC 1" >> ${.TARGET}
.if ${MK_VIMAGE_SUPPORT} != "no"
	@echo "#define VIMAGE 1" >> ${.TARGET}
.endif
# Note: Define 'options' in DEFAULTS to 1. For simplicity, no check if the
# option is in opt_global.h. Nearly all the options in DEFAUlTS today are in
# opt_global.h with GEOM_* being the main exceptions. Move any options from
# GENERIC or std.* files to DEFAULTS to get this treatment for untied builds.
	@awk '$$1 == "options" && $$2 !~ "GEOM_" { print "#define ", $$2, " 1"; }' \
		< ${SYSDIR}/${MACHINE}/conf/DEFAULTS \
		>>  ${.TARGET}
.if ${MK_BHYVE_SNAPSHOT} != "no"
opt_bhyve_snapshot.h:
	@echo "#define BHYVE_SNAPSHOT 1" > ${.TARGET}
.endif
opt_bpf.h:
	echo "#define DEV_BPF 1" > ${.TARGET}
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
	@echo "#define TCP_OFFLOAD 1" >> ${.TARGET}
.endif
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.if ${MK_IPSEC_SUPPORT} != "no"
opt_ipsec.h:
	@echo "#define IPSEC_SUPPORT 1" > ${.TARGET}
.endif
.if ${MK_RATELIMIT} != "no"
opt_ratelimit.h:
	@echo "#define RATELIMIT 1" > ${.TARGET}
.endif
opt_mrouting.h:
	@echo "#define MROUTING 1" > ${.TARGET}
.if ${MK_FDT} != "no"
opt_platform.h:
	@echo "#define FDT 1" > ${.TARGET}
.endif
opt_printf.h:
	echo "#define PRINTF_BUFR_SIZE 128" > ${.TARGET}
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > ${.TARGET}
.if ${MK_SCTP_SUPPORT} != "no"
opt_sctp.h:
	@echo "#define SCTP_SUPPORT 1" > ${.TARGET}
.endif
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
	echo "#define IEEE80211_SUPPORT_MESH 1" >> ${.TARGET}
KERN_OPTS.i386=NEW_PCIB DEV_PCI
KERN_OPTS.amd64=NEW_PCIB DEV_PCI
KERN_OPTS.powerpc=NEW_PCIB DEV_PCI
KERN_OPTS=MROUTING IEEE80211_DEBUG \
	IEEE80211_SUPPORT_MESH DEV_BPF \
	${KERN_OPTS.${MACHINE}} ${KERN_OPTS_EXTRA}
.if ${MK_BHYVE_SNAPSHOT} != "no"
KERN_OPTS+= BHYVE_SNAPSHOT
.endif
.if ${MK_INET_SUPPORT} != "no"
KERN_OPTS+= INET TCP_OFFLOAD
.endif
.if ${MK_INET6_SUPPORT} != "no"
KERN_OPTS+= INET6
.endif
.if ${MK_IPSEC_SUPPORT} != "no"
KERN_OPTS+= IPSEC_SUPPORT
.endif
.if ${MK_SCTP_SUPPORT} != "no"
KERN_OPTS+= SCTP_SUPPORT
.endif
.elif !defined(KERN_OPTS)
# Add all the options that are mentioned in any opt_*.h file when we
# have a kernel build directory to pull them from.
KERN_OPTS!=cat ${KERNBUILDDIR}/opt*.h | awk '{print $$2;}' | sort -u
.export KERN_OPTS
.endif

.if !defined(NO_MODULES) && !defined(__MPATH) && !make(install) && \
    (empty(.MAKEFLAGS:M-V) || defined(NO_SKIP_MPATH))
__MPATH!=find ${SYSDIR:tA}/ -name \*_if.m
.export __MPATH
.endif
