.. _kadm5_hook_plugin:

KADM5 hook interface (kadm5_hook)
=================================

The kadm5_hook interface allows modules to perform actions when
changes are made to the Kerberos database through :ref:`kadmin(1)`.
For a detailed description of the kadm5_hook interface, see the header
file ``<krb5/kadm5_hook_plugin.h>``.

The kadm5_hook interface has five primary methods: **chpass**,
**create**, **modify**, **remove**, and **rename**.  (The **rename**
method was introduced in release 1.14.)  Each of these methods is
called twice when the corresponding administrative action takes place,
once before the action is committed and once afterwards.  A module can
prevent the action from taking place by returning an error code during
the pre-commit stage.

A module can create and destroy per-process state objects by
implementing the **init** and **fini** methods.  State objects have
the type kadm5_hook_modinfo, which is an abstract pointer type.  A
module should typically cast this to an internal type for the state
object.

Because the kadm5_hook interface is tied closely to the kadmin
interface (which is explicitly unstable), it may not remain as stable
across versions as other public pluggable interfaces.
