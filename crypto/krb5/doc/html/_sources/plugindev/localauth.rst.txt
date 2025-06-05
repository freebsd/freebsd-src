.. _localauth_plugin:

Local authorization interface (localauth)
=========================================

The localauth interface was first introduced in release 1.12.  It
allows modules to control the relationship between Kerberos principals
and local system accounts.  When an application calls
:c:func:`krb5_kuserok` or :c:func:`krb5_aname_to_localname`, localauth
modules are consulted to determine the result.  For a detailed
description of the localauth interface, see the header file
``<krb5/localauth_plugin.h>``.

A module can create and destroy per-library-context state objects
using the **init** and **fini** methods.  If the module does not need
any state, it does not need to implement these methods.

The optional **userok** method allows a module to control the behavior
of :c:func:`krb5_kuserok`.  The module receives the authenticated name
and the local account name as inputs, and can return either 0 to
authorize access, KRB5_PLUGIN_NO_HANDLE to defer the decision to other
modules, or another error (canonically EPERM) to authoritatively deny
access.  Access is granted if at least one module grants access and no
module authoritatively denies access.

The optional **an2ln** method can work in two different ways.  If the
module sets an array of uppercase type names in **an2ln_types**, then
the module's **an2ln** method will only be invoked by
:c:func:`krb5_aname_to_localname` if an **auth_to_local** value in
:ref:`krb5.conf(5)` refers to one of the module's types.  In this
case, the *type* and *residual* arguments will give the type name and
residual string of the **auth_to_local** value.

If the module does not set **an2ln_types** but does implement
**an2ln**, the module's **an2ln** method will be invoked for all
:c:func:`krb5_aname_to_localname` operations unless an earlier module
determines a mapping, with *type* and *residual* set to NULL.  The
module can return KRB5_LNAME_NO_TRANS to defer mapping to later
modules.

If a module implements **an2ln**, it must also implement
**free_string** to ensure that memory is allocated and deallocated
consistently.
