# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.14.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SHLIB_MAJOR=	6
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
