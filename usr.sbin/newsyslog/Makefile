#	$Id: Makefile,v 1.2 1996/01/16 10:32:01 graichen Exp $

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
