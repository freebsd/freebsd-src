#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD$

PROG=	rarpd
MAN=	rarpd.8

CFLAGS+= -DTFTP_DIR=\"/tftpboot\"
WARNS?=	2
# This breaks with format strings returned by expand_syslog_m().. argh!
#FORMAT_AUDIT?=	1

.include <bsd.prog.mk>
