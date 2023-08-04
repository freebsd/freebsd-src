.. _kadm5_auth_plugin:

kadmin authorization interface (kadm5_auth)
===========================================

The kadm5_auth interface (new in release 1.16) allows modules to
determine whether a client principal is authorized to perform an
operation in the kadmin protocol, and to apply restrictions to
principal operations.  For a detailed description of the kadm5_auth
interface, see the header file ``<krb5/kadm5_auth_plugin.h>``.

A module can create and destroy per-process state objects by
implementing the **init** and **fini** methods.  State objects have
the type kadm5_auth_modinfo, which is an abstract pointer type.  A
module should typically cast this to an internal type for the state
object.

The kadm5_auth interface has one method for each kadmin operation,
with parameters specific to the operation.  Each method can return
either 0 to authorize access, KRB5_PLUGIN_NO_HANDLE to defer the
decision to other modules, or another error (canonically EPERM) to
authoritatively deny access.  Access is granted if at least one module
grants access and no module authoritatively denies access.

The **addprinc** and **modprinc** methods can also impose restrictions
on the principal operation by returning a ``struct
kadm5_auth_restrictions`` object.  The module should also implement
the **free_restrictions** method if it dynamically allocates
restrictions objects for principal operations.

kadm5_auth modules can optionally inspect principal or policy objects.
To do this, the module must also include ``<kadm5/admin.h>`` to gain
access to the structure definitions for those objects.  As the kadmin
interface is explicitly not as stable as other public interfaces,
modules which do this may not retain compatibility across releases.
