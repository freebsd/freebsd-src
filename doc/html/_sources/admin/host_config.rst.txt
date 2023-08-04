Host configuration
==================

All hosts running Kerberos software, whether they are clients,
application servers, or KDCs, can be configured using
:ref:`krb5.conf(5)`.  Here we describe some of the behavior changes
you might want to make.


Default realm
-------------

In the :ref:`libdefaults` section, the **default_realm** realm
relation sets the default Kerberos realm.  For example::

    [libdefaults]
        default_realm = ATHENA.MIT.EDU

The default realm affects Kerberos behavior in the following ways:

* When a principal name is parsed from text, the default realm is used
  if no ``@REALM`` component is specified.

* The default realm affects login authorization as described below.

* For programs which operate on a Kerberos database, the default realm
  is used to determine which database to operate on, unless the **-r**
  parameter is given to specify a realm.

* A server program may use the default realm when looking up its key
  in a :ref:`keytab file <keytab_file>`, if its realm is not
  determined by :ref:`domain_realm` configuration or by the server
  program itself.

* If :ref:`kinit(1)` is passed the **-n** flag, it requests anonymous
  tickets from the default realm.

In some situations, these uses of the default realm might conflict.
For example, it might be desirable for principal name parsing to use
one realm by default, but for login authorization to use a second
realm.  In this situation, the first realm can be configured as the
default realm, and **auth_to_local** relations can be used as
described below to use the second realm for login authorization.


.. _login_authorization:

Login authorization
-------------------

If a host runs a Kerberos-enabled login service such as OpenSSH with
GSSAPIAuthentication enabled, login authorization rules determine
whether a Kerberos principal is allowed to access a local account.

By default, a Kerberos principal is allowed access to an account if
its realm matches the default realm and its name matches the account
name.  (For historical reasons, access is also granted by default if
the name has two components and the second component matches the
default realm; for instance, ``alice/ATHENA.MIT.EDU@ATHENA.MIT.EDU``
is granted access to the ``alice`` account if ``ATHENA.MIT.EDU`` is
the default realm.)

The simplest way to control local access is using :ref:`.k5login(5)`
files.  To use these, place a ``.k5login`` file in the home directory
of each account listing the principal names which should have login
access to that account.  If it is not desirable to use ``.k5login``
files located in account home directories, the **k5login_directory**
relation in the :ref:`libdefaults` section can specify a directory
containing one file per account uname.

By default, if a ``.k5login`` file is present, it controls
authorization both positively and negatively--any principal name
contained in the file is granted access and any other principal name
is denied access, even if it would have had access if the ``.k5login``
file didn't exist.  The **k5login_authoritative** relation in the
:ref:`libdefaults` section can be set to false to make ``.k5login``
files provide positive authorization only.

The **auth_to_local** relation in the :ref:`realms` section for the
default realm can specify pattern-matching rules to control login
authorization.  For example, the following configuration allows access
to principals from a different realm than the default realm::

    [realms]
        DEFAULT.REALM = {
            # Allow access to principals from OTHER.REALM.
            #
            # [1:$1@$0] matches single-component principal names and creates
            # a selection string containing the principal name and realm.
            #
            # (.*@OTHER\.REALM) matches against the selection string, so that
            # only principals in OTHER.REALM are matched.
            #
            # s/@OTHER\.REALM$// removes the realm name, leaving behind the
            # principal name as the account name.
            auth_to_local = RULE:[1:$1@$0](.*@OTHER\.REALM)s/@OTHER\.REALM$//

            # Also allow principals from the default realm.  Omit this line
            # to only allow access to principals in OTHER.REALM.
            auth_to_local = DEFAULT
        }

The **auth_to_local_names** subsection of the :ref:`realms` section
for the default realm can specify explicit mappings from principal
names to local accounts.  The key used in this subsection is the
principal name without realm, so it is only safe to use in a Kerberos
environment with a single realm or a tightly controlled set of realms.
An example use of **auth_to_local_names** might be::

    [realms]
        ATHENA.MIT.EDU = {
            auth_to_local_names = {
                # Careful, these match principals in any realm!
                host/example.com = hostaccount
                fred = localfred
            }
        }

Local authorization behavior can also be modified using plugin
modules; see :ref:`hostrealm_plugin` for details.


.. _plugin_config:

Plugin module configuration
---------------------------

Many aspects of Kerberos behavior, such as client preauthentication
and KDC service location, can be modified through the use of plugin
modules.  For most of these behaviors, you can use the :ref:`plugins`
section of krb5.conf to register third-party modules, and to switch
off registered or built-in modules.

A plugin module takes the form of a Unix shared object
(``modname.so``) or Windows DLL (``modname.dll``).  If you have
installed a third-party plugin module and want to register it, you do
so using the **module** relation in the appropriate subsection of the
[plugins] section.  The value for **module** must give the module name
and the path to the module, separated by a colon.  The module name
will often be the same as the shared object's name, but in unusual
cases (such as a shared object which implements multiple modules for
the same interface) it might not be.  For example, to register a
client preauthentication module named ``mypreauth`` installed at
``/path/to/mypreauth.so``, you could write::

    [plugins]
        clpreauth = {
            module = mypreauth:/path/to/mypreauth.so
        }

Many of the pluggable behaviors in MIT krb5 contain built-in modules
which can be switched off.  You can disable a built-in module (or one
you have registered) using the **disable** directive in the
appropriate subsection of the [plugins] section.  For example, to
disable the use of .k5identity files to select credential caches, you
could write::

    [plugins]
        ccselect = {
            disable = k5identity
        }

If you want to disable multiple modules, specify the **disable**
directive multiple times, giving one module to disable each time.

Alternatively, you can explicitly specify which modules you want to be
enabled for that behavior using the **enable_only** directive.  For
example, to make :ref:`kadmind(8)` check password quality using only a
module you have registered, and no other mechanism, you could write::

    [plugins]
        pwqual = {
            module = mymodule:/path/to/mymodule.so
            enable_only = mymodule
        }

Again, if you want to specify multiple modules, specify the
**enable_only** directive multiple times, giving one module to enable
each time.

Some Kerberos interfaces use different mechanisms to register plugin
modules.


KDC location modules
~~~~~~~~~~~~~~~~~~~~

For historical reasons, modules to control how KDC servers are located
are registered simply by placing the shared object or DLL into the
"libkrb5" subdirectory of the krb5 plugin directory, which defaults to
|libdir|\ ``/krb5/plugins``.  For example, Samba's winbind krb5
locator plugin would be registered by placing its shared object in
|libdir|\ ``/krb5/plugins/libkrb5/winbind_krb5_locator.so``.


.. _gssapi_plugin_config:

GSSAPI mechanism modules
~~~~~~~~~~~~~~~~~~~~~~~~

GSSAPI mechanism modules are registered using the file
|sysconfdir|\ ``/gss/mech`` or configuration files in the
|sysconfdir|\ ``/gss/mech.d`` directory with a ``.conf``
suffix.  Each line in these files has the form::

    name  oid  pathname  [options]  <type>

Only the name, oid, and pathname are required.  *name* is the
mechanism name, which may be used for debugging or logging purposes.
*oid* is the object identifier of the GSSAPI mechanism to be
registered.  *pathname* is a path to the module shared object or DLL.
*options* (if present) are options provided to the plugin module,
surrounded in square brackets.  *type* (if present) can be used to
indicate a special type of module.  Currently the only special module
type is "interposer", for a module designed to intercept calls to
other mechanisms.

If the environment variable **GSS_MECH_CONFIG** is set, its value is
used as the sole mechanism configuration filename.


.. _profile_plugin_config:

Configuration profile modules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A configuration profile module replaces the information source for
:ref:`krb5.conf(5)` itself.  To use a profile module, begin krb5.conf
with the line::

    module PATHNAME:STRING

where *PATHNAME* is a path to the module shared object or DLL, and
*STRING* is a string to provide to the module.  The module will then
take over, and the rest of krb5.conf will be ignored.
