obj-$(CONFIG_INFINIBAND_QLGC_VNIC)		+= qlgc_vnic.o

qlgc_vnic-y					:= vnic_main.o \
						   vnic_ib.o \
						   vnic_viport.o \
						   vnic_control.o \
						   vnic_data.o \
						   vnic_netpath.o \
						   vnic_config.o \
						   vnic_sys.o \
						   vnic_multicast.o

qlgc_vnic-$(CONFIG_INFINIBAND_QLGC_VNIC_STATS)	+= vnic_stats.o
