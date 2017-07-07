.. _klist(1):

klist
=====

SYNOPSIS
--------

**klist**
[**-e**]
[[**-c**] [**-l**] [**-A**] [**-f**] [**-s**] [**-a** [**-n**]]]
[**-C**]
[**-k** [**-t**] [**-K**]]
[**-V**]
[*cache_name*\|\ *keytab_name*]


DESCRIPTION
-----------

klist lists the Kerberos principal and Kerberos tickets held in a
credentials cache, or the keys held in a keytab file.


OPTIONS
-------

**-e**
    Displays the encryption types of the session key and the ticket
    for each credential in the credential cache, or each key in the
    keytab file.

**-l**
    If a cache collection is available, displays a table summarizing
    the caches present in the collection.

**-A**
    If a cache collection is available, displays the contents of all
    of the caches in the collection.

**-c**
    List tickets held in a credentials cache. This is the default if
    neither **-c** nor **-k** is specified.

**-f**
    Shows the flags present in the credentials, using the following
    abbreviations::

        F    Forwardable
        f    forwarded
        P    Proxiable
        p    proxy
        D    postDateable
        d    postdated
        R    Renewable
        I    Initial
        i    invalid
        H    Hardware authenticated
        A    preAuthenticated
        T    Transit policy checked
        O    Okay as delegate
        a    anonymous

**-s**
    Causes klist to run silently (produce no output).  klist will exit
    with status 1 if the credentials cache cannot be read or is
    expired, and with status 0 otherwise.

**-a**
    Display list of addresses in credentials.

**-n**
    Show numeric addresses instead of reverse-resolving addresses.

**-C**
    List configuration data that has been stored in the credentials
    cache when klist encounters it.  By default, configuration data
    is not listed.

**-k**
    List keys held in a keytab file.

**-i**
    In combination with **-k**, defaults to using the default client
    keytab instead of the default acceptor keytab, if no name is
    given.

**-t**
    Display the time entry timestamps for each keytab entry in the
    keytab file.

**-K**
    Display the value of the encryption key in each keytab entry in
    the keytab file.

**-V**
    Display the Kerberos version number and exit.

If *cache_name* or *keytab_name* is not specified, klist will display
the credentials in the default credentials cache or keytab file as
appropriate.  If the **KRB5CCNAME** environment variable is set, its
value is used to locate the default ticket cache.


ENVIRONMENT
-----------

klist uses the following environment variable:

**KRB5CCNAME**
    Location of the default Kerberos 5 credentials (ticket) cache, in
    the form *type*:*residual*.  If no *type* prefix is present, the
    **FILE** type is assumed.  The type of the default cache may
    determine the availability of a cache collection; for instance, a
    default cache of type **DIR** causes caches within the directory
    to be present in the collection.


FILES
-----

|ccache|
    Default location of Kerberos 5 credentials cache

|keytab|
    Default location for the local host's keytab file.


SEE ALSO
--------

:ref:`kinit(1)`, :ref:`kdestroy(1)`
