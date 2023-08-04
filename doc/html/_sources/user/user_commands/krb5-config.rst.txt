.. _krb5-config(1):

krb5-config
===========

SYNOPSIS
--------

**krb5-config**
[**-**\ **-help** | **-**\ **-all** | **-**\ **-version** | **-**\ **-vendor** | **-**\ **-prefix** | **-**\ **-exec-prefix** | **-**\ **-defccname** | **-**\ **-defktname** | **-**\ **-defcktname** | **-**\ **-cflags** | **-**\ **-libs** [*libraries*]]


DESCRIPTION
-----------

krb5-config tells the application programmer what flags to use to compile
and link programs against the installed Kerberos libraries.


OPTIONS
-------

**-**\ **-help**
    prints a usage message.  This is the default behavior when no options
    are specified.

**-**\ **-all**
    prints the version, vendor, prefix, and exec-prefix.

**-**\ **-version**
    prints the version number of the Kerberos installation.

**-**\ **-vendor**
    prints the name of the vendor of the Kerberos installation.

**-**\ **-prefix**
    prints the prefix for which the Kerberos installation was built.

**-**\ **-exec-prefix**
    prints the prefix for executables for which the Kerberos installation
    was built.

**-**\ **-defccname**
    prints the built-in default credentials cache location.

**-**\ **-defktname**
    prints the built-in default keytab location.

**-**\ **-defcktname**
    prints the built-in default client (initiator) keytab location.

**-**\ **-cflags**
    prints the compilation flags used to build the Kerberos installation.

**-**\ **-libs** [*library*]
    prints the compiler options needed to link against *library*.
    Allowed values for *library* are:

    ============  ===============================================
    krb5          Kerberos 5 applications (default)
    gssapi        GSSAPI applications with Kerberos 5 bindings
    kadm-client   Kadmin client
    kadm-server   Kadmin server
    kdb           Applications that access the Kerberos database
    ============  ===============================================

EXAMPLES
--------

krb5-config is particularly useful for compiling against a Kerberos
installation that was installed in a non-standard location.  For example,
a Kerberos installation that is installed in ``/opt/krb5/`` but uses
libraries in ``/usr/local/lib/`` for text localization would produce
the following output::

    shell% krb5-config --libs krb5
    -L/opt/krb5/lib -Wl,-rpath -Wl,/opt/krb5/lib -L/usr/local/lib -lkrb5 -lk5crypto -lcom_err


SEE ALSO
--------

:ref:`kerberos(7)`, cc(1)
