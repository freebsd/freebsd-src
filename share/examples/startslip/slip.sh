#!/bin/sh
# $FreeBSD: src/share/examples/startslip/slip.sh,v 1.3.20.1 2009/04/15 03:14:26 kensmith Exp $
startslip -b 57600 -U ./slup.sh -D ./sldown.sh \
	-s atd<phone1> -s atd<phone2> -s atd<phone3> \
	-h -t 60 -w 2 -W 20 /dev/cuad1 <login> <password>
