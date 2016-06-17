#
# Makefile for the USB serial device drivers.
#

O_TARGET	:= usb-serial.o

# Object file lists.

obj-$(CONFIG_USB_SERIAL)                        += usbserial.o
obj-$(CONFIG_USB_SERIAL_VISOR)			+= visor.o
obj-$(CONFIG_USB_SERIAL_IPAQ)			+= ipaq.o
obj-$(CONFIG_USB_SERIAL_WHITEHEAT)		+= whiteheat.o
obj-$(CONFIG_USB_SERIAL_FTDI_SIO)		+= ftdi_sio.o
obj-$(CONFIG_USB_SERIAL_KEYSPAN_PDA)		+= keyspan_pda.o
obj-$(CONFIG_USB_SERIAL_XIRCOM)			+= keyspan_pda.o
obj-$(CONFIG_USB_SERIAL_KEYSPAN)		+= keyspan.o
obj-$(CONFIG_USB_SERIAL_OMNINET)		+= omninet.o
obj-$(CONFIG_USB_SERIAL_DIGI_ACCELEPORT)	+= digi_acceleport.o
obj-$(CONFIG_USB_SERIAL_BELKIN)			+= belkin_sa.o
obj-$(CONFIG_USB_SERIAL_EMPEG)			+= empeg.o
obj-$(CONFIG_USB_SERIAL_MCT_U232)		+= mct_u232.o
obj-$(CONFIG_USB_SERIAL_EDGEPORT)		+= io_edgeport.o
obj-$(CONFIG_USB_SERIAL_EDGEPORT_TI)		+= io_ti.o
obj-$(CONFIG_USB_SERIAL_PL2303)			+= pl2303.o
obj-$(CONFIG_USB_SERIAL_CYBERJACK)		+= cyberjack.o
obj-$(CONFIG_USB_SERIAL_IR)			+= ir-usb.o
obj-$(CONFIG_USB_SERIAL_KLSI)			+= kl5kusb105.o
obj-$(CONFIG_USB_SERIAL_KOBIL_SCT)		+= kobil_sct.o

# Objects that export symbols.
export-objs	:= usbserial.o

include $(TOPDIR)/Rules.make

