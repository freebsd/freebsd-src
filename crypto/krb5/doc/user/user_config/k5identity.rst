.. _.k5identity(5):

.k5identity
===========

DESCRIPTION
-----------

The .k5identity file, which resides in a user's home directory,
contains a list of rules for selecting a client principals based on
the server being accessed.  These rules are used to choose a
credential cache within the cache collection when possible.

Blank lines and lines beginning with ``#`` are ignored.  Each line has
the form:

    *principal* *field*\=\ *value* ...

If the server principal meets all of the field constraints, then
principal is chosen as the client principal.  The following fields are
recognized:

**realm**
    If the realm of the server principal is known, it is matched
    against *value*, which may be a pattern using shell wildcards.
    For host-based server principals, the realm will generally only be
    known if there is a :ref:`domain_realm` section in
    :ref:`krb5.conf(5)` with a mapping for the hostname.

**service**
    If the server principal is a host-based principal, its service
    component is matched against *value*, which may be a pattern using
    shell wildcards.

**host**
    If the server principal is a host-based principal, its hostname
    component is converted to lower case and matched against *value*,
    which may be a pattern using shell wildcards.

    If the server principal matches the constraints of multiple lines
    in the .k5identity file, the principal from the first matching
    line is used.  If no line matches, credentials will be selected
    some other way, such as the realm heuristic or the current primary
    cache.


EXAMPLE
-------

The following example .k5identity file selects the client principal
``alice@KRBTEST.COM`` if the server principal is within that realm,
the principal ``alice/root@EXAMPLE.COM`` if the server host is within
a servers subdomain, and the principal ``alice/mail@EXAMPLE.COM`` when
accessing the IMAP service on ``mail.example.com``::

    alice@KRBTEST.COM       realm=KRBTEST.COM
    alice/root@EXAMPLE.COM  host=*.servers.example.com
    alice/mail@EXAMPLE.COM  host=mail.example.com service=imap


SEE ALSO
--------

kerberos(1), :ref:`krb5.conf(5)`
