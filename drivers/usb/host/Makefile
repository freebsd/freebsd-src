#
# Makefile for USB Host Controller Driver
# framework and drivers
#

O_TARGET	:=

obj-$(CONFIG_USB_EHCI_HCD)			+= ehci-hcd.o
obj-$(CONFIG_USB_UHCI_ALT)			+= uhci.o
obj-$(CONFIG_USB_UHCI)				+= usb-uhci.o
obj-$(CONFIG_USB_OHCI)				+= usb-ohci.o
obj-$(CONFIG_USB_SL811HS_ALT)  			+= sl811.o
obj-$(CONFIG_USB_SL811HS)  			+= hc_sl811.o

# Extract lists of the multi-part drivers.
# The 'int-*' lists are the intermediate files used to build the multi's.
multi-y		:= $(filter $(list-multi), $(obj-y))
multi-m		:= $(filter $(list-multi), $(obj-m))
int-y		:= $(sort $(foreach m, $(multi-y), $($(basename $(m))-objs)))
int-m		:= $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))

# Take multi-part drivers out of obj-y and put components in.
obj-y		:= $(filter-out $(list-multi), $(obj-y)) $(int-y)

# Translate to Rules.make lists.
OX_OBJS		:= $(obj-y)
MX_OBJS		:= $(obj-m)
MIX_OBJS	:= $(int-m)

include $(TOPDIR)/Rules.make
