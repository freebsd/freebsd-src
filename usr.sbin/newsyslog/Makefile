#	$Id: Makefile,v 1.1.1.1 1996/01/05 09:28:11 graichen Exp $

PROG=	newsyslog

CFLAGS+= -DOSF
CFLAGS+= -DCONF=\"/etc/newsyslog.conf\"
CFLAGS+= -DPIDFILE=\"/var/run/syslog.pid\"
CFLAGS+= -DCOMPRESS_PATH=\"/usr/bin/gzip\"
CFLAGS+= -DCOMPRESS_PROG=\"gzip\"
CFLAGS+= -DCOMPRESS_POSTFIX=\".gz\"

BINOWN=	root

MAN8=	newsyslog.8

.include <bsd.prog.mk>
