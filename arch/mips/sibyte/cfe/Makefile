USE_STANDARD_AS_RULE := true

L_TARGET = cfe.a

obj-y			= cfe_api.o setup.o
obj-$(CONFIG_SMP)	+= smp.o
obj-$(CONFIG_SIBYTE_CFE_CONSOLE)	+= console.o

include $(TOPDIR)/Rules.make
