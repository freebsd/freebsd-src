default:
.if exists(/etc/mail/ips.txt)
	mv -f /etc/mail/ips.txt /etc/mail/ips.txt.0
.endif
	fetch ftp://ftp.gulf.net/pub/docs/ips.txt
	cat ips.txt | cut -d ' ' -f 1 | \
		sed -e 's/$$/ 	#blocked. contact postmaster/' > \
		/etc/mail/ips.txt
.if exists(/etc/mail/domains.txt)
	mv -f /etc/mail/domains.txt /etc/mail/domains.txt.0
.endif
	fetch ftp://ftp.gulf.net/pub/docs/domains.txt
	cat domains.txt | cut -d ' ' -f 1 | \
		sed -e 's/$$/ 	#blocked. contact postmaster/' > \
		/etc/mail/domains.txt

install:
	makemap hash /etc/mail/spamsites < /etc/mail/domains.txt
.if exists(/etc/mail/spamsites.local)
	makemap -o hash /etc/mail/spamsites < /etc/mail/spamsites.local
.endif
	makemap hash /etc/mail/denyip < /etc/mail/ips.txt
.if exists(/etc/mail/denyip.local)
	makemap -o hash /etc/mail/denyip < /etc/mail/denyip.local
.endif

all: default install
