# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.13.12.1 2010/12/21 17:10:29 kensmith Exp $

SHLIB_MAJOR=	5
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
