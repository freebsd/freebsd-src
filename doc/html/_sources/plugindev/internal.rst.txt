Internal pluggable interfaces
=============================

Following are brief discussions of pluggable interfaces which have not
yet been made public.  These interfaces are functional, but the
interfaces are likely to change in incompatible ways from release to
release.  In some cases, it may be necessary to copy header files from
the krb5 source tree to use an internal interface.  Use these with
care, and expect to need to update your modules for each new release
of MIT krb5.


Kerberos database interface (KDB)
---------------------------------

A KDB module implements a database back end for KDC principal and
policy information, and can also control many aspects of KDC behavior.
For a full description of the interface, see the header file
``<kdb.h>``.

The KDB pluggable interface is often referred to as the DAL (Database
Access Layer).


Authorization data interface (authdata)
---------------------------------------

The authdata interface allows a module to provide (from the KDC) or
consume (in application servers) authorization data of types beyond
those handled by the core MIT krb5 code base.  The interface is
defined in the header file ``<krb5/authdata_plugin.h>``, which is not
installed by the build.
