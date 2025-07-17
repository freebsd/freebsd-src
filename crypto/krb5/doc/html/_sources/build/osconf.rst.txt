osconf.hin
==========

There is one configuration file which you may wish to edit to control
various compile-time parameters in the Kerberos distribution::

    include/osconf.hin

The list that follows is by no means complete, just some of the more
interesting variables.

**DEFAULT_PROFILE_PATH**
    The pathname to the file which contains the profiles for the known
    realms, their KDCs, etc. The default value is |krb5conf|.
**DEFAULT_KEYTAB_NAME**
    The type and pathname to the default server keytab file.  The
    default is |keytab|.
**DEFAULT_KDC_ENCTYPE**
    The default encryption type for the KDC database master key.  The
    default value is |defmkey|.
**RCTMPDIR**
    The directory which stores replay caches.  The default is
    ``/var/tmp``.
**DEFAULT_KDB_FILE**
    The location of the default database.  The default value is
    |kdcdir|\ ``/principal``.
