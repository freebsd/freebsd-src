.. _kswitch(1):

kswitch
=======

SYNOPSIS
--------

**kswitch**
{**-c** *cachename*\|\ **-p** *principal*}


DESCRIPTION
-----------

kswitch makes the specified credential cache the primary cache for the
collection, if a cache collection is available.


OPTIONS
-------

**-c** *cachename*
    Directly specifies the credential cache to be made primary.

**-p** *principal*
    Causes the cache collection to be searched for a cache containing
    credentials for *principal*.  If one is found, that collection is
    made primary.


ENVIRONMENT
-----------

kswitch uses the following environment variables:

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


SEE ALSO
--------

:ref:`kinit(1)`, :ref:`kdestroy(1)`, :ref:`klist(1)`), kerberos(1)
