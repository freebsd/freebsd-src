#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: src/usr.sbin/rarpd/Makefile,v 1.12 2001/07/20 06:20:16 obrien Exp $

PROG=	rarpd
MAN=	rarpd.8

WARNS?=	2
# This breaks with format strings returned by expand_syslog_m().. argh!
#FORMAT_AUDIT?=	1
CFLAGS+= -DTFTP_DIR=\"/tftpboot\"

.include <bsd.prog.mk>
