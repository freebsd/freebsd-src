.. _pwqual_plugin:

Password quality interface (pwqual)
===================================

The pwqual interface allows modules to control what passwords are
allowed when a user changes passwords.  For a detailed description of
the pwqual interface, see the header file ``<krb5/pwqual_plugin.h>``.

The primary pwqual method is **check**, which receives a password as
input and returns success (0) or a ``KADM5_PASS_Q_`` failure code
depending on whether the password is allowed.  The **check** method
also receives the principal name and the name of the principal's
password policy as input; although there is no stable interface for
the module to obtain the fields of the password policy, it can define
its own configuration or data store based on the policy name.

A module can create and destroy per-process state objects by
implementing the **open** and **close** methods.  State objects have
the type krb5_pwqual_moddata, which is an abstract pointer type.  A
module should typically cast this to an internal type for the state
object.  The **open** method also receives the name of the realm's
dictionary file (as configured by the **dict_file** variable in the
:ref:`kdc_realms` section of :ref:`kdc.conf(5)`) if it wishes to use
it.
