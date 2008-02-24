# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.13 2007/06/18 18:47:54 rafan Exp $

SHLIB_MAJOR=	5
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
