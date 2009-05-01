# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.13.8.1 2009/04/15 03:14:26 kensmith Exp $

SHLIB_MAJOR=	5
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
