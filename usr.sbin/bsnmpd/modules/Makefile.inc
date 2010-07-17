# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.14.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SHLIB_MAJOR=	6
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
