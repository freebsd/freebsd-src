KDC cookie format
=================

:rfc:`6113` section 5.2 specifies a pa-data type PA-FX-COOKIE, which
clients are required to reflect back to the KDC during
pre-authentication.  The MIT krb5 KDC uses the following formats for
cookies.


Trivial cookie (version 0)
--------------------------

If there is no pre-authentication mechanism state information to save,
a trivial cookie containing the value "MIT" is used.  A trivial cookie
is needed to indicate that the conversation can continue.


Secure cookie (version 1)
-------------------------

In release 1.14 and later, a secure cookie can be sent if there is any
mechanism state to save for the next request.  A secure cookie
contains the concatenation of the following:

* the four bytes "MIT1"
* a four-byte big-endian kvno value
* an :rfc:`3961` ciphertext

The ciphertext is encrypted in the cookie key with key usage
number 513.  The cookie key is derived from a key in the local krbtgt
principal entry for the realm (e.g. ``krbtgt/KRBTEST.COM@KRBTEST.COM``
if the request is to the ``KRBTEST.COM`` realm).  The first krbtgt key
for the indicated kvno value is combined with the client principal as
follows::

    cookie-key <- random-to-key(PRF+(tgt-key, "COOKIE" | client-princ))

where **random-to-key** is the :rfc:`3961` random-to-key operation for
the krbtgt key's encryption type, **PRF+** is defined in :rfc:`6113`,
and ``|`` denotes concatenation.  *client-princ* is the request client
principal name with realm, marshalled according to :rfc:`1964` section
2.1.1.

The plain text of the encrypted part of a cookie is the DER encoding
of the following ASN.1 type::

    SecureCookie ::= SEQUENCE {
        time     INTEGER,
        data     SEQUENCE OF PA-DATA,
        ...
    }

The time field represents the cookie creation time; for brevity, it is
encoded as an integer giving the POSIX timestamp rather than as an
ASN.1 GeneralizedTime value.  The data field contains one element for
each pre-authentication type which requires saved state.  For
mechanisms which have separate request and reply types, the request
type is used; this allows the KDC to determine whether a cookie is
relevant to a request by comparing the request pa-data types to the
cookie data types.
