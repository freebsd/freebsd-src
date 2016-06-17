#
# Makefile for USB peripheral controller and gadget drivers
# for kbuild 2.4
#

# for static linking
O_TARGET	:= built-in.o

list-multi			:= g_zero.o g_ether.o g_file_storage.o

obj-$(CONFIG_USB_NET2280)	+= net2280.o
obj-$(CONFIG_USB_GOKU)		+= goku_udc.o

# only one of these may be statically linked ...
controller-$(CONFIG_USB_NET2280)	+= net2280.o
controller-$(CONFIG_USB_GOKU)		+= goku_udc.o

# ... and only one of these, too; kbuild/kconfig don't help though.
g_zero-objs			:= zero.o usbstring.o config.o
obj-$(CONFIG_USB_ZERO)		+= g_zero.o

g_ether-objs			:= ether.o usbstring.o config.o
obj-$(CONFIG_USB_ETH)		+= g_ether.o

g_file_storage-objs		:= file_storage.o usbstring.o
obj-$(CONFIG_USB_FILE_STORAGE)	+= g_file_storage.o

export-objs :=			$(controller-y) $(controller-m)

include $(TOPDIR)/Rules.make

g_zero.o: $(g_zero-objs)
	$(LD) -r -o $@ $(g_zero-objs)
g_ether.o: $(g_ether-objs)
	$(LD) -r -o $@ $(g_ether-objs)
g_file_storage.o: $(g_file_storage-objs)
	$(LD) -r -o $@ $(g_file_storage-objs)
