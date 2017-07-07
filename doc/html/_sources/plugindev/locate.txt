Server location interface (locate)
==================================

The locate interface allows modules to control how KDCs and similar
services are located by clients.  For a detailed description of the
ccselect interface, see the header file ``<krb5/locate_plugin.h>``.

.. note: The locate interface does not follow the normal conventions
         for MIT krb5 pluggable interfaces, because it was made public
         before those conventions were established.

A locate module exports a structure object of type
krb5plugin_service_locate_ftable, with the name ``service_locator``.
The structure contains a minor version and pointers to the module's
methods.

The primary locate method is **lookup**, which accepts a service type,
realm name, desired socket type, and desired address family (which
will be AF_UNSPEC if no specific address family is desired).  The
method should invoke the callback function once for each server
address it wants to return, passing a socket type (SOCK_STREAM for TCP
or SOCK_DGRAM for UDP) and socket address.  The **lookup** method
should return 0 if it has authoritatively determined the server
addresses for the realm, KRB5_PLUGIN_NO_HANDLE if it wants to let
other location mechanisms determine the server addresses, or another
code if it experienced a failure which should abort the location
process.

A module can create and destroy per-library-context state objects by
implementing the **init** and **fini** methods.  State objects have
the type void \*, and should be cast to an internal type for the state
object.
