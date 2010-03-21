# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.13.10.1 2010/02/10 00:26:20 kensmith Exp $

SHLIB_MAJOR=	5
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
