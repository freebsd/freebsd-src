#	$Id: Makefile,v 1.6 1999/01/31 15:21:15 jmb Exp $

install:
	/usr/sbin/makemap hash /etc/mail/spamsites < /etc/mail/block_domains.txt
.if exists(/etc/mail/spamsites.local)
	/usr/sbin/makemap -o hash /etc/mail/spamsites < /etc/mail/spamsites.local
.endif
	/usr/sbin/makemap hash /etc/mail/denyip < /etc/mail/block_ips.txt
.if exists(/etc/mail/denyip.local)
	/usr/sbin/makemap -o hash /etc/mail/denyip < /etc/mail/denyip.local
.endif

all: install
