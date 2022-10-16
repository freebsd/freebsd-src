#!/usr/bin/env bash

# This script was used to generate the broken signed zones used for testing.

# Override the current date; it is used in Unbound's configuration also.
NOW=20010101

# differentiate for MacOS with "gdate"
DATE=date
which gdate > /dev/null 2>&1 && DATE=gdate

ONEMONTHAGO=`$DATE -d "$NOW - 1 month" +%Y%m%d`
ONEMONTH=`$DATE -d "$NOW + 1 month" +%Y%m%d`
YESTERDAY=`$DATE -d "$NOW - 2 days" +%Y%m%d`
TOMORROW=`$DATE -d "$NOW + 2 days" +%Y%m%d`

# Root trust anchor
echo ". IN DS 20326 8 2 e06d44b80b8f1d39a95c0b0d7c65d08458e880409bbc683457104237c7f8ec8d" > bogus/trust-anchors

# create oudated zones
CSK=`ldns-keygen -a ECDSAP256SHA256 -k -r /dev/urandom dnssec-failures.test`
echo $CSK
cat $CSK.ds >> bogus/trust-anchors

ldns-signzone -i $YESTERDAY -e $ONEMONTH -f - bogus/dnssec-failures.test $CSK | \
	grep -v '^missingrrsigs\.dnssec-failures\.test\..*IN.*RRSIG.*TXT' | \
	sed 's/Signatures invalid/Signatures INVALID/g' | \
	grep -v '^notyetincepted\.dnssec-failures\.test\..*IN.*TXT' | \
	grep -v '^notyetincepted\.dnssec-failures\.test\..*IN.*RRSIG.*TXT' | \
	grep -v '^expired\.dnssec-failures\.test\..*IN.*TXT' | \
	grep -v '^expired\.dnssec-failures\.test\..*IN.*RRSIG.*TXT' > base
ldns-signzone -i $ONEMONTHAGO -e $YESTERDAY -f - bogus/dnssec-failures.test $CSK | \
	grep -v '[	]NSEC[	]' | \
	grep '^expired\.dnssec-failures\.test\..*IN.*TXT' > expired
ldns-signzone -i $TOMORROW -e $ONEMONTH -f - bogus/dnssec-failures.test $CSK | \
	grep -v '[	]NSEC[	]' | \
	grep '^notyetincepted\.dnssec-failures\.test\..*IN.*TXT' > notyetincepted

cat base expired notyetincepted > bogus/dnssec-failures.test.signed

# cleanup old zone keys
rm -f $CSK.*

# create zone with DNSKEY missing
CSK=`ldns-keygen -a ECDSAP256SHA256 -k -r /dev/urandom dnskey-failures.test`
echo $CSK
cat $CSK.ds >> bogus/trust-anchors

ldns-signzone -i $YESTERDAY -e $ONEMONTH -f tmp.signed bogus/dnskey-failures.test $CSK
grep -v '	DNSKEY	' tmp.signed > bogus/dnskey-failures.test.signed

# cleanup old zone keys
rm -f $CSK.*

# create zone with NSEC missing
CSK=`ldns-keygen -a ECDSAP256SHA256 -k -r /dev/urandom nsec-failures.test`
echo $CSK
cat $CSK.ds >> bogus/trust-anchors

ldns-signzone -i $YESTERDAY -e $ONEMONTH -f tmp.signed bogus/nsec-failures.test $CSK
grep -v '	NSEC	' tmp.signed > bogus/nsec-failures.test.signed

# cleanup old zone keys
rm -f $CSK.*

# create zone with RRSIGs missing
CSK=`ldns-keygen -a ECDSAP256SHA256 -k -r /dev/urandom rrsig-failures.test`
echo $CSK
cat $CSK.ds >> bogus/trust-anchors

ldns-signzone -i $YESTERDAY -e $ONEMONTH -f tmp.signed bogus/rrsig-failures.test $CSK
grep -v '	RRSIG	' tmp.signed > bogus/rrsig-failures.test.signed

# cleanup
rm -f base expired notyetincepted tmp.signed $CSK.*
