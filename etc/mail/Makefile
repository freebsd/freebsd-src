default:
.if exists(/etc/mail/ips.txt)
	mv -f /etc/mail/ips.txt /etc/mail/ips.txt.0
.endif
	fetch ftp://ftp.gulf.net/pub/docs/ips.txt
	sort ips.txt > block_ips.txt
	touch allowsorted_ips.txt
.if exists(/etc/mail/allow_ips.txt)
	sort allow_ips.txt > allowsorted_ips.txt
.endif
	comm -23 block_ips.txt allowsorted_ips.txt | \
	        cut -d ' ' -f 1 | \
		sed -e 's/$$/ 	#blocked. contact postmaster/' > \
		/etc/mail/block_ips.new
	rm allowsorted_ips.txt
	mv /etc/mail/block_ips.new /etc/mail/block_ips.txt
.if exists(/etc/mail/domains.txt)
	mv -f /etc/mail/domains.txt /etc/mail/domains.txt.0
.endif
	fetch ftp://ftp.gulf.net/pub/docs/domains.txt
	sort domains.txt > block_domains.txt
	touch allowsorted_domains.txt
.if exists(/etc/mail/allow_domains.txt)
	sort allow_domains.txt > allowsorted_domains.txt
.endif
	comm -23 block_domains.txt allowsorted_domains.txt | \
		cut -d ' ' -f 1 | \
		sed -e 's/$$/ 	#blocked. contact postmaster/' > \
		/etc/mail/block_domains.new
	rm allowsorted_domains.txt
	mv /etc/mail/block_domains.new /etc/mail/block_domains.txt

install:
	makemap hash /etc/mail/spamsites < /etc/mail/block_domains.txt
.if exists(/etc/mail/spamsites.local)
	makemap -o hash /etc/mail/spamsites < /etc/mail/spamsites.local
.endif
	makemap hash /etc/mail/denyip < /etc/mail/block_ips.txt
.if exists(/etc/mail/denyip.local)
	makemap -o hash /etc/mail/denyip < /etc/mail/denyip.local
.endif

all: default install
