.. _kinit(1):

kinit
=====

SYNOPSIS
--------

**kinit**
[**-V**]
[**-l** *lifetime*]
[**-s** *start_time*]
[**-r** *renewable_life*]
[**-p** | -**P**]
[**-f** | -**F**]
[**-a**]
[**-A**]
[**-C**]
[**-E**]
[**-v**]
[**-R**]
[**-k** [-**t** *keytab_file*]]
[**-c** *cache_name*]
[**-n**]
[**-S** *service_name*]
[**-I** *input_ccache*]
[**-T** *armor_ccache*]
[**-X** *attribute*\ [=\ *value*]]
[*principal*]


DESCRIPTION
-----------

kinit obtains and caches an initial ticket-granting ticket for
*principal*.  If *principal* is absent, kinit chooses an appropriate
principal name based on existing credential cache contents or the
local username of the user invoking kinit.  Some options modify the
choice of principal name.


OPTIONS
-------

**-V**
    display verbose output.

**-l** *lifetime*
    (:ref:`duration` string.)  Requests a ticket with the lifetime
    *lifetime*.

    For example, ``kinit -l 5:30`` or ``kinit -l 5h30m``.

    If the **-l** option is not specified, the default ticket lifetime
    (configured by each site) is used.  Specifying a ticket lifetime
    longer than the maximum ticket lifetime (configured by each site)
    will not override the configured maximum ticket lifetime.

**-s** *start_time*
    (:ref:`duration` string.)  Requests a postdated ticket.  Postdated
    tickets are issued with the **invalid** flag set, and need to be
    resubmitted to the KDC for validation before use.

    *start_time* specifies the duration of the delay before the ticket
    can become valid.

**-r** *renewable_life*
    (:ref:`duration` string.)  Requests renewable tickets, with a total
    lifetime of *renewable_life*.

**-f**
    requests forwardable tickets.

**-F**
    requests non-forwardable tickets.

**-p**
    requests proxiable tickets.

**-P**
    requests non-proxiable tickets.

**-a**
    requests tickets restricted to the host's local address[es].

**-A**
    requests tickets not restricted by address.

**-C**
    requests canonicalization of the principal name, and allows the
    KDC to reply with a different client principal from the one
    requested.

**-E**
    treats the principal name as an enterprise name (implies the
    **-C** option).

**-v**
    requests that the ticket-granting ticket in the cache (with the
    **invalid** flag set) be passed to the KDC for validation.  If the
    ticket is within its requested time range, the cache is replaced
    with the validated ticket.

**-R**
    requests renewal of the ticket-granting ticket.  Note that an
    expired ticket cannot be renewed, even if the ticket is still
    within its renewable life.

    Note that renewable tickets that have expired as reported by
    :ref:`klist(1)` may sometimes be renewed using this option,
    because the KDC applies a grace period to account for client-KDC
    clock skew.  See :ref:`krb5.conf(5)` **clockskew** setting.

**-k** [**-i** | **-t** *keytab_file*]
    requests a ticket, obtained from a key in the local host's keytab.
    The location of the keytab may be specified with the **-t**
    *keytab_file* option, or with the **-i** option to specify the use
    of the default client keytab; otherwise the default keytab will be
    used.  By default, a host ticket for the local host is requested,
    but any principal may be specified.  On a KDC, the special keytab
    location ``KDB:`` can be used to indicate that kinit should open
    the KDC database and look up the key directly.  This permits an
    administrator to obtain tickets as any principal that supports
    authentication based on the key.

**-n**
    Requests anonymous processing.  Two types of anonymous principals
    are supported.

    For fully anonymous Kerberos, configure pkinit on the KDC and
    configure **pkinit_anchors** in the client's :ref:`krb5.conf(5)`.
    Then use the **-n** option with a principal of the form ``@REALM``
    (an empty principal name followed by the at-sign and a realm
    name).  If permitted by the KDC, an anonymous ticket will be
    returned.

    A second form of anonymous tickets is supported; these
    realm-exposed tickets hide the identity of the client but not the
    client's realm.  For this mode, use ``kinit -n`` with a normal
    principal name.  If supported by the KDC, the principal (but not
    realm) will be replaced by the anonymous principal.

    As of release 1.8, the MIT Kerberos KDC only supports fully
    anonymous operation.

**-I** *input_ccache*

    Specifies the name of a credentials cache that already contains a
    ticket.  When obtaining that ticket, if information about how that
    ticket was obtained was also stored to the cache, that information
    will be used to affect how new credentials are obtained, including
    preselecting the same methods of authenticating to the KDC.

**-T** *armor_ccache*
    Specifies the name of a credentials cache that already contains a
    ticket.  If supported by the KDC, this cache will be used to armor
    the request, preventing offline dictionary attacks and allowing
    the use of additional preauthentication mechanisms.  Armoring also
    makes sure that the response from the KDC is not modified in
    transit.

**-c** *cache_name*
    use *cache_name* as the Kerberos 5 credentials (ticket) cache
    location.  If this option is not used, the default cache location
    is used.

    The default cache location may vary between systems.  If the
    **KRB5CCNAME** environment variable is set, its value is used to
    locate the default cache.  If a principal name is specified and
    the type of the default cache supports a collection (such as the
    DIR type), an existing cache containing credentials for the
    principal is selected or a new one is created and becomes the new
    primary cache.  Otherwise, any existing contents of the default
    cache are destroyed by kinit.

**-S** *service_name*
    specify an alternate service name to use when getting initial
    tickets.

**-X** *attribute*\ [=\ *value*]
    specify a pre-authentication *attribute* and *value* to be
    interpreted by pre-authentication modules.  The acceptable
    attribute and value values vary from module to module.  This
    option may be specified multiple times to specify multiple
    attributes.  If no value is specified, it is assumed to be "yes".

    The following attributes are recognized by the PKINIT
    pre-authentication mechanism:

    **X509_user_identity**\ =\ *value*
        specify where to find user's X509 identity information

    **X509_anchors**\ =\ *value*
        specify where to find trusted X509 anchor information

    **flag_RSA_PROTOCOL**\ [**=yes**]
        specify use of RSA, rather than the default Diffie-Hellman
        protocol


ENVIRONMENT
-----------

kinit uses the following environment variables:

**KRB5CCNAME**
    Location of the default Kerberos 5 credentials cache, in the form
    *type*:*residual*.  If no *type* prefix is present, the **FILE**
    type is assumed.  The type of the default cache may determine the
    availability of a cache collection; for instance, a default cache
    of type **DIR** causes caches within the directory to be present
    in the collection.


FILES
-----

|ccache|
    default location of Kerberos 5 credentials cache

|keytab|
    default location for the local host's keytab.


SEE ALSO
--------

:ref:`klist(1)`, :ref:`kdestroy(1)`, kerberos(1)
