PKINIT freshness tokens
=======================

:rfc:`8070` specifies a pa-data type PA_AS_FRESHNESS, which clients
should reflect within signed PKINIT data to prove recent access to the
client certificate private key.  The contents of a freshness token are
left to the KDC implementation.  The MIT krb5 KDC uses the following
format for freshness tokens (starting in release 1.17):

* a four-byte big-endian POSIX timestamp
* a four-byte big-endian key version number
* an :rfc:`3961` checksum, with no ASN.1 wrapper

The checksum is computed using the first key in the local krbtgt
principal entry for the realm (e.g. ``krbtgt/KRBTEST.COM@KRBTEST.COM``
if the request is to the ``KRBTEST.COM`` realm) of the indicated key
version.  The checksum type must be the mandatory checksum type for
the encryption type of the krbtgt key.  The key usage value for the
checksum is 514.
