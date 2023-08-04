.. _kdestroy(1):

kdestroy
========

SYNOPSIS
--------

**kdestroy**
[**-A**]
[**-q**]
[**-c** *cache_name*]
[**-p** *princ_name*]


DESCRIPTION
-----------

The kdestroy utility destroys the user's active Kerberos authorization
tickets by overwriting and deleting the credentials cache that
contains them.  If the credentials cache is not specified, the default
credentials cache is destroyed.


OPTIONS
-------

**-A**
    Destroys all caches in the collection, if a cache collection is
    available.  May be used with the **-c** option to specify the
    collection to be destroyed.

**-q**
    Run quietly.  Normally kdestroy beeps if it fails to destroy the
    user's tickets.  The **-q** flag suppresses this behavior.

**-c** *cache_name*
    Use *cache_name* as the credentials (ticket) cache name and
    location; if this option is not used, the default cache name and
    location are used.

    The default credentials cache may vary between systems.  If the
    **KRB5CCNAME** environment variable is set, its value is used to
    name the default ticket cache.

**-p** *princ_name*
    If a cache collection is available, destroy the cache for
    *princ_name* instead of the primary cache.  May be used with the
    **-c** option to specify the collection to be searched.


NOTE
----

Most installations recommend that you place the kdestroy command in
your .logout file, so that your tickets are destroyed automatically
when you log out.


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

:ref:`kinit(1)`, :ref:`klist(1)`, :ref:`kerberos(7)`
