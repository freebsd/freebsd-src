# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.14.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

SHLIB_MAJOR=	6
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
