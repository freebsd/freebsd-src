#!/bin/sh

openssl asn1parse -genconf est-csrattrs.cnf -out est-csrattrs.der -oid hs20.oid
base64 est-csrattrs.der > est-attrs.b64
