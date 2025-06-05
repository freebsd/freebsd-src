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

See :ref:`kerberos(7)` for a description of Kerberos environment
variables.


FILES
-----

|ccache|
    Default location of Kerberos 5 credentials cache


SEE ALSO
--------

:ref:`kinit(1)`, :ref:`kdestroy(1)`, :ref:`klist(1)`,
:ref:`kerberos(7)`
