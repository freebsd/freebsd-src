.. _kvno(1):

kvno
====

SYNOPSIS
--------

**kvno**
[**-c** *ccache*]
[**-e** *etype*]
[**-k** *keytab*]
[**-q**]
[**-u** | **-S** *sname*]
[**-P**]
[**--cached-only**]
[**--no-store**]
[**--out-cache** *cache*]
[[{**-F** *cert_file* | {**-I** | **-U**} *for_user*} [**-P**]] | **--u2u** *ccache*]
*service1 service2* ...


DESCRIPTION
-----------

kvno acquires a service ticket for the specified Kerberos principals
and prints out the key version numbers of each.


OPTIONS
-------

**-c** *ccache*
    Specifies the name of a credentials cache to use (if not the
    default)

**-e** *etype*
    Specifies the enctype which will be requested for the session key
    of all the services named on the command line.  This is useful in
    certain backward compatibility situations.

**-k** *keytab*
    Decrypt the acquired tickets using *keytab* to confirm their
    validity.

**-q**
    Suppress printing output when successful.  If a service ticket
    cannot be obtained, an error message will still be printed and
    kvno will exit with nonzero status.

**-u**
    Use the unknown name type in requested service principal names.
    This option Cannot be used with *-S*.

**-P**
    Specifies that the *service1 service2* ...  arguments are to be
    treated as services for which credentials should be acquired using
    constrained delegation.  This option is only valid when used in
    conjunction with protocol transition.

**-S** *sname*
    Specifies that the *service1 service2* ... arguments are
    interpreted as hostnames, and the service principals are to be
    constructed from those hostnames and the service name *sname*.
    The service hostnames will be canonicalized according to the usual
    rules for constructing service principals.

**-I** *for_user*
    Specifies that protocol transition (S4U2Self) is to be used to
    acquire a ticket on behalf of *for_user*.  If constrained
    delegation is not requested, the service name must match the
    credentials cache client principal.

**-U** *for_user*
    Same as -I, but treats *for_user* as an enterprise name.

**-F** *cert_file*
    Specifies that protocol transition is to be used, identifying the
    client principal with the X.509 certificate in *cert_file*.  The
    certificate file must be in PEM format.

**--cached-only**
    Only retrieve credentials already present in the cache, not from
    the KDC.  (Added in release 1.19.)

**--no-store**
    Do not store retrieved credentials in the cache.  If
    **--out-cache** is also specified, credentials will still be
    stored into the output credential cache.  (Added in release 1.19.)

**--out-cache** *ccache*
    Initialize *ccache* and store all retrieved credentials into it.
    Do not store acquired credentials in the input cache.  (Added in
    release 1.19.)

**--u2u** *ccache*
    Requests a user-to-user ticket.  *ccache* must contain a local
    krbtgt ticket for the server principal.  The reported version
    number will typically be 0, as the resulting ticket is not
    encrypted in the server's long-term key.

ENVIRONMENT
-----------

See :ref:`kerberos(7)` for a description of Kerberos environment
variables.


FILES
-----

|ccache|
    Default location of the credentials cache


SEE ALSO
--------

:ref:`kinit(1)`, :ref:`kdestroy(1)`, :ref:`kerberos(7)`
