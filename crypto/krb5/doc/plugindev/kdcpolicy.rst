.. _kdcpolicy_plugin:

KDC policy interface (kdcpolicy)
================================

The kdcpolicy interface was first introduced in release 1.16.  It
allows modules to veto otherwise valid AS and TGS requests or restrict
the lifetime and renew time of the resulting ticket.  For a detailed
description of the kdcpolicy interface, see the header file
``<krb5/kdcpolicy_plugin.h>``.

The optional **check_as** and **check_tgs** functions allow the module
to perform access control.  Additionally, a module can create and
destroy module data with the **init** and **fini** methods.  Module
data objects last for the lifetime of the KDC process, and are
provided to all other methods.  The data has the type
krb5_kdcpolicy_moddata, which should be cast to the appropriate
internal type.

kdcpolicy modules can optionally inspect principal entries.  To do
this, the module must also include ``<kdb.h>`` to gain access to the
principal entry structure definition.  As the KDB interface is
explicitly not as stable as other public interfaces, modules which do
this may not retain compatibility across releases.
