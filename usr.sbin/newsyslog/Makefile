#	$Id: Makefile,v 1.4 1997/02/22 16:08:24 peter Exp $

PROG=	newsyslog

CFLAGS+= -DOSF
CFLAGS+= -DCONF=\"/etc/newsyslog.conf\"
CFLAGS+= -DPIDFILE=\"/var/run/syslog.pid\"
CFLAGS+= -DCOMPRESS_PATH=\"/usr/bin/gzip\"
CFLAGS+= -DCOMPRESS_PROG=\"gzip\"
CFLAGS+= -DCOMPRESS_POSTFIX=\".gz\"

MAN8=	newsyslog.8

.include <bsd.prog.mk>
