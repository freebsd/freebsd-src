# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $

SHLIB_MAJOR=	5
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
