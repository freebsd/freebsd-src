# $FreeBSD$
#
# Common code to marry kernel config(8) goo and module building goo.
#

# Generate options files that otherwise would be built
# in substantially similar ways through the tree. Move
# the code here when they all produce identical results
# (or should)
.if !defined(KERNBUILDDIR)
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
.if ${MK_EISA} != "no"
opt_eisa.h:
	@echo "#define DEV_EISA 1" > ${.TARGET}
.endif
opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}
opt_natm.h:
	echo "#define NATM 1" > ${.TARGET}
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > ${.TARGET}
opt_wlan.h:
	echo "#define IEEE80211_DEBUG 1" > ${.TARGET}
	echo "#define IEEE80211_AMPDU_AGE 1" >> ${.TARGET}
	echo "#define IEEE80211_SUPPORT_MESH 1" >> ${.TARGET}
KERN_OPTS.i386=NEW_PCIB DEV_PCI
KERN_OPTS.pc98=NEW_PCIB DEV_PCI
KERN_OPTS.amd64=NEW_PCIB DEV_PCI
KERN_OPTS.powerpc=NEW_PCIB DEV_PCI
KERN_OPTS=MROUTING NATM IEEE80211_DEBUG \
	IEEE80211_AMPDU_AGE IEEE80211_SUPPORT_MESH DEV_BPF \
	${KERN_OPTS.${MACHINE}} ${KERN_OPTS_EXTRA}
.if ${MK_INET_SUPPORT} != "no"
KERN_OPTS+= INET TCP_OFFLOAD
.endif
.if ${MK_INET6_SUPPORT} != "no"
KERN_OPTS+= INET6
.endif
.if ${MK_EISA} != "no"
KERN_OPTS+= DEV_EISA
.endif
.elif !defined(KERN_OPTS)
KERN_OPTS!=cat ${KERNBUILDDIR}/opt*.h | awk '{print $$2;}' | sort -u
.export KERN_OPTS
.endif

.if !defined(NO_MODULES) && !defined(__MPATH)
__MPATH!=find ${SYSDIR:tA}/ -name \*_if.m
.export __MPATH
.endif
