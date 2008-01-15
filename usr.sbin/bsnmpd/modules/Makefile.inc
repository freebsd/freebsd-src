# $FreeBSD: src/usr.sbin/bsnmpd/modules/Makefile.inc,v 1.9.2.2 2006/01/25 13:22:58 harti Exp $

SHLIB_MAJOR=	4
WARNS?=		6

MANFILTER=	sed -e 's%@MODPATH@%${LIBDIR}/%g'		\
		    -e 's%@DEFPATH@%${DEFSDIR}/%g'		\
		    -e 's%@MIBSPATH@%${BMIBSDIR}/%g'
