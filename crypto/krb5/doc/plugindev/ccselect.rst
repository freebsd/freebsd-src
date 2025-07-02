.. _ccselect_plugin:

Credential cache selection interface (ccselect)
===============================================

The ccselect interface allows modules to control how credential caches
are chosen when a GSSAPI client contacts a service.  For a detailed
description of the ccselect interface, see the header file
``<krb5/ccselect_plugin.h>``.

The primary ccselect method is **choose**, which accepts a server
principal as input and returns a ccache and/or principal name as
output.  A module can use the krb5_cccol APIs to iterate over the
cache collection in order to find an appropriate ccache to use.

.. TODO: add reference to the admin guide for ccaches and cache
   collections when we have appropriate sections.

A module can create and destroy per-library-context state objects by
implementing the **init** and **fini** methods.  State objects have
the type krb5_ccselect_moddata, which is an abstract pointer type.  A
module should typically cast this to an internal type for the state
object.

A module can have one of two priorities, "authoritative" or
"heuristic".  Results from authoritative modules, if any are
available, will take priority over results from heuristic modules.  A
module communicates its priority as a result of the **init** method.
