.. _hostrealm_plugin:

Host-to-realm interface (hostrealm)
===================================

The host-to-realm interface was first introduced in release 1.12.  It
allows modules to control the local mapping of hostnames to realm
names as well as the default realm.  For a detailed description of the
hostrealm interface, see the header file
``<krb5/hostrealm_plugin.h>``.

Although the mapping methods in the hostrealm interface return a list
of one or more realms, only the first realm in the list is currently
used by callers.  Callers may begin using later responses in the
future.

Any mapping method may return KRB5_PLUGIN_NO_HANDLE to defer
processing to a later module.

A module can create and destroy per-library-context state objects
using the **init** and **fini** methods.  If the module does not need
any state, it does not need to implement these methods.

The optional **host_realm** method allows a module to determine
authoritative realm mappings for a hostname.  The first authoritative
mapping is used in preference to KDC referrals when getting service
credentials.

The optional **fallback_realm** method allows a module to determine
fallback mappings for a hostname.  The first fallback mapping is tried
if there is no authoritative mapping for a realm, and KDC referrals
failed to produce a successful result.

The optional **default_realm** method allows a module to determine the
local default realm.

If a module implements any of the above methods, it must also
implement **free_list** to ensure that memory is allocated and
deallocated consistently.
