#
# $FreeBSD$
#


S=${.CURDIR}/../../..

all:
	awk -f $S/tools/usbdevs2h.awk $S/dev/usb/usbdevs -d ; mv usbdevs_data.h usb2_devtable.h
	awk -f $S/tools/usbdevs2h.awk $S/dev/usb/usbdevs -h ; mv usbdevs.h usb2_devid.h

clean:
	rm -f usb2_devtable.h usb2_devid.h

