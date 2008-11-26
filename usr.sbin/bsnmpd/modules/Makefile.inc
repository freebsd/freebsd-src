# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.9.2.2.8.1 2008/10/02 02:57:24 kensmith Exp $

SHLIB_MAJOR=	4
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
