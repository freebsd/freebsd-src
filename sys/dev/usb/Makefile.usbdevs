# The files usbdevs.h and usbdevs_data.h are generated from usbdevs
#
# $FreeBSD: src/sys/dev/usb/Makefile.usbdevs,v 1.1.2.2 2000/05/07 14:36:41 n_hibma Exp $

AWK=	awk
UNAME=	uname
RM=	rm

# The targets are always remade.

.PHONY=	all

all:	usbdevs devlist2h.awk
	/bin/rm -f usbdevs.h usbdevs_data.h
	${AWK} -v type=USB -v os=`${UNAME} -s` -f devlist2h.awk usbdevs

