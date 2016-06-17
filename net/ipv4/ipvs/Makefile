#
# Makefile for the IPVS modules on top of IPv4.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET :=	ipvs.o

export-objs :=	ip_vs_core.o ip_vs_app.o

ip_vs-objs :=	ip_vs_conn.o ip_vs_core.o ip_vs_ctl.o ip_vs_sched.o \
		ip_vs_app.o ip_vs_sync.o ip_vs_est.o

ifeq ($(CONFIG_IP_VS),y)
  obj-y := $(ip_vs-objs)
else
  ifeq ($(CONFIG_IP_VS),m)
    obj-m := ip_vs.o
  endif
endif

# IPVS schedulers
obj-$(CONFIG_IP_VS_RR) += ip_vs_rr.o
obj-$(CONFIG_IP_VS_WRR) += ip_vs_wrr.o
obj-$(CONFIG_IP_VS_LC) += ip_vs_lc.o
obj-$(CONFIG_IP_VS_WLC) += ip_vs_wlc.o
obj-$(CONFIG_IP_VS_LBLC) += ip_vs_lblc.o
obj-$(CONFIG_IP_VS_LBLCR) += ip_vs_lblcr.o
obj-$(CONFIG_IP_VS_DH) += ip_vs_dh.o
obj-$(CONFIG_IP_VS_SH) += ip_vs_sh.o
obj-$(CONFIG_IP_VS_SED) += ip_vs_sed.o
obj-$(CONFIG_IP_VS_NQ) += ip_vs_nq.o

# IPVS application helpers
obj-$(CONFIG_IP_VS_FTP) += ip_vs_ftp.o

include $(TOPDIR)/Rules.make

ip_vs.o: $(ip_vs-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(ip_vs-objs)
