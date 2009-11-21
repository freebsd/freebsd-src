dnl $Id: acinclude.m4 13337 2004-02-12 14:19:16Z lha $
dnl $FreeBSD: src/crypto/heimdal/acinclude.m4,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
dnl
dnl Only put things that for some reason can't live in the `cf'
dnl directory in this file.
dnl

dnl $xId: misc.m4,v 1.1 1997/12/14 15:59:04 joda Exp $
dnl
m4_define([upcase],`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`)dnl
