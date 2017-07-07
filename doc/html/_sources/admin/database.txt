Database administration
=======================

A Kerberos database contains all of a realm's Kerberos principals,
their passwords, and other administrative information about each
principal.  For the most part, you will use the :ref:`kdb5_util(8)`
program to manipulate the Kerberos database as a whole, and the
:ref:`kadmin(1)` program to make changes to the entries in the
database.  (One notable exception is that users will use the
:ref:`kpasswd(1)` program to change their own passwords.)  The kadmin
program has its own command-line interface, to which you type the
database administrating commands.

:ref:`kdb5_util(8)` provides a means to create, delete, load, or dump
a Kerberos database.  It also contains commands to roll over the
database master key, and to stash a copy of the key so that the
:ref:`kadmind(8)` and :ref:`krb5kdc(8)` daemons can use the database
without manual input.

:ref:`kadmin(1)` provides for the maintenance of Kerberos principals,
password policies, and service key tables (keytabs).  Normally it
operates as a network client using Kerberos authentication to
communicate with :ref:`kadmind(8)`, but there is also a variant, named
kadmin.local, which directly accesses the Kerberos database on the
local filesystem (or through LDAP).  kadmin.local is necessary to set
up enough of the database to be able to use the remote version.

kadmin can authenticate to the admin server using the service
principal ``kadmin/HOST`` (where *HOST* is the hostname of the admin
server) or ``kadmin/admin``.  If the credentials cache contains a
ticket for either service principal and the **-c** ccache option is
specified, that ticket is used to authenticate to KADM5.  Otherwise,
the **-p** and **-k** options are used to specify the client Kerberos
principal name used to authenticate.  Once kadmin has determined the
principal name, it requests a ``kadmin/admin`` Kerberos service ticket
from the KDC, and uses that service ticket to authenticate to KADM5.

See :ref:`kadmin(1)` for the available kadmin and kadmin.local
commands and options.


kadmin options
--------------

You can invoke :ref:`kadmin(1)` or kadmin.local with any of the
following options:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  kadmin_synopsis:
   :end-before: kadmin_synopsis_end:

**OPTIONS**

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _kadmin_options:
   :end-before: _kadmin_options_end:


Date Format
-----------

For the supported date-time formats see :ref:`getdate` section
in :ref:`datetime`.


Principals
----------

Each entry in the Kerberos database contains a Kerberos principal and
the attributes and policies associated with that principal.


.. _add_mod_del_princs:

Adding, modifying and deleting principals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add a principal to the database, use the :ref:`kadmin(1)`
**add_principal** command.

To modify attributes of a principal, use the kadmin
**modify_principal** command.

To delete a principal, use the kadmin **delete_principal** command.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _add_principal:
   :end-before: _add_principal_end:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _modify_principal:
   :end-before: _modify_principal_end:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _delete_principal:
   :end-before: _delete_principal_end:


Examples
########

If you want to create a principal which is contained by a LDAP object,
all you need to do is::

    kadmin: addprinc -x dn=cn=jennifer,dc=example,dc=com jennifer
    WARNING: no policy specified for "jennifer@ATHENA.MIT.EDU";
    defaulting to no policy.
    Enter password for principal jennifer@ATHENA.MIT.EDU:  <= Type the password.
    Re-enter password for principal jennifer@ATHENA.MIT.EDU:  <=Type it again.
    Principal "jennifer@ATHENA.MIT.EDU" created.
    kadmin:

If you want to create a principal under a specific LDAP container and
link to an existing LDAP object, all you need to do is::

    kadmin: addprinc -x containerdn=dc=example,dc=com -x linkdn=cn=david,dc=example,dc=com david
    WARNING: no policy specified for "david@ATHENA.MIT.EDU";
    defaulting to no policy.
    Enter password for principal david@ATHENA.MIT.EDU:  <= Type the password.
    Re-enter password for principal david@ATHENA.MIT.EDU:  <=Type it again.
    Principal "david@ATHENA.MIT.EDU" created.
    kadmin:

If you want to associate a ticket policy to a principal, all you need
to do is::

    kadmin: modprinc -x tktpolicy=userpolicy david
    Principal "david@ATHENA.MIT.EDU" modified.
    kadmin:

If, on the other hand, you want to set up an account that expires on
January 1, 2000, that uses a policy called "stduser", with a temporary
password (which you want the user to change immediately), you would
type the following::

    kadmin: addprinc david -expire "1/1/2000 12:01am EST" -policy stduser +needchange
    Enter password for principal david@ATHENA.MIT.EDU:  <= Type the password.
    Re-enter password for principal
    david@ATHENA.MIT.EDU:  <= Type it again.
    Principal "david@ATHENA.MIT.EDU" created.
    kadmin:

If you want to delete a principal::

    kadmin: delprinc jennifer
    Are you sure you want to delete the principal
    "jennifer@ATHENA.MIT.EDU"? (yes/no): yes
    Principal "jennifer@ATHENA.MIT.EDU" deleted.
    Make sure that you have removed this principal from
    all ACLs before reusing.
    kadmin:


Retrieving information about a principal
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To retrieve a listing of the attributes and/or policies associated
with a principal, use the :ref:`kadmin(1)` **get_principal** command.

To generate a listing of principals, use the kadmin
**list_principals** command.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _get_principal:
   :end-before: _get_principal_end:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _list_principals:
   :end-before: _list_principals_end:


Changing passwords
~~~~~~~~~~~~~~~~~~

To change a principal's password use the :ref:`kadmin(1)`
**change_password** command.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _change_password:
   :end-before: _change_password_end:

.. note::

          Password changes through kadmin are subject to the same
          password policies as would apply to password changes through
          :ref:`kpasswd(1)`.


.. _policies:

Policies
--------

A policy is a set of rules governing passwords.  Policies can dictate
minimum and maximum password lifetimes, minimum number of characters
and character classes a password must contain, and the number of old
passwords kept in the database.


Adding, modifying and deleting policies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To add a new policy, use the :ref:`kadmin(1)` **add_policy** command.

To modify attributes of a principal, use the kadmin **modify_policy**
command.

To delete a policy, use the kadmin **delete_policy** command.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _add_policy:
   :end-before: _add_policy_end:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _modify_policy:
   :end-before: _modify_policy_end:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _delete_policy:
   :end-before: _delete_policy_end:

.. note::

          You must cancel the policy from *all* principals before
          deleting it.  The *delete_policy* command will fail if the policy
          is in use by any principals.


Retrieving policies
~~~~~~~~~~~~~~~~~~~

To retrieve a policy, use the :ref:`kadmin(1)` **get_policy** command.

You can retrieve the list of policies with the kadmin
**list_policies** command.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _get_policy:
   :end-before: _get_policy_end:

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _list_policies:
   :end-before: _list_policies_end:


Policies and principals
~~~~~~~~~~~~~~~~~~~~~~~

Policies can be applied to principals as they are created by using
the **-policy** flag to :ref:`add_principal`. Existing principals can
be modified by using the **-policy** or **-clearpolicy** flag to
:ref:`modify_principal`.


Updating the history key
~~~~~~~~~~~~~~~~~~~~~~~~

If a policy specifies a number of old keys kept of two or more, the
stored old keys are encrypted in a history key, which is found in the
key data of the ``kadmin/history`` principal.

Currently there is no support for proper rollover of the history key,
but you can change the history key (for example, to use a better
encryption type) at the cost of invalidating currently stored old
keys.  To change the history key, run::

    kadmin: change_password -randkey kadmin/history

This command will fail if you specify the **-keepold** flag.  Only one
new history key will be created, even if you specify multiple key/salt
combinations.

In the future, we plan to migrate towards encrypting old keys in the
master key instead of the history key, and implementing proper
rollover support for stored old keys.


.. _privileges:

Privileges
----------

Administrative privileges for the Kerberos database are stored in the
file :ref:`kadm5.acl(5)`.

.. note::

          A common use of an admin instance is so you can grant
          separate permissions (such as administrator access to the
          Kerberos database) to a separate Kerberos principal. For
          example, the user ``joeadmin`` might have a principal for
          his administrative use, called ``joeadmin/admin``.  This
          way, ``joeadmin`` would obtain ``joeadmin/admin`` tickets
          only when he actually needs to use those permissions.


.. _db_operations:

Operations on the Kerberos database
-----------------------------------

The :ref:`kdb5_util(8)` command is the primary tool for administrating
the Kerberos database.

.. include:: admin_commands/kdb5_util.rst
   :start-after:  _kdb5_util_synopsis:
   :end-before: _kdb5_util_synopsis_end:

**OPTIONS**

.. include:: admin_commands/kdb5_util.rst
   :start-after:  _kdb5_util_options:
   :end-before: _kdb5_util_options_end:

.. toctree::
   :maxdepth: 1


Dumping a Kerberos database to a file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To dump a Kerberos database into a file, use the :ref:`kdb5_util(8)`
**dump** command on one of the KDCs.

.. include:: admin_commands/kdb5_util.rst
   :start-after:  _kdb5_util_dump:
   :end-before: _kdb5_util_dump_end:


Examples
########

::

    shell% kdb5_util dump dumpfile
    shell%

    shell% kbd5_util dump -verbose dumpfile
    kadmin/admin@ATHENA.MIT.EDU
    krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU
    kadmin/history@ATHENA.MIT.EDU
    K/M@ATHENA.MIT.EDU
    kadmin/changepw@ATHENA.MIT.EDU
    shell%

If you specify which principals to dump, you must use the full
principal, as in the following example::

    shell% kdb5_util dump -verbose dumpfile K/M@ATHENA.MIT.EDU kadmin/admin@ATHENA.MIT.EDU
    kadmin/admin@ATHENA.MIT.EDU
    K/M@ATHENA.MIT.EDU
    shell%

Otherwise, the principals will not match those in the database and
will not be dumped::

     shell% kdb5_util dump -verbose dumpfile K/M kadmin/admin
     shell%

If you do not specify a dump file, kdb5_util will dump the database to
the standard output.


.. _restore_from_dump:

Restoring a Kerberos database from a dump file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To restore a Kerberos database dump from a file, use the
:ref:`kdb5_util(8)` **load** command on one of the KDCs.

.. include:: admin_commands/kdb5_util.rst
   :start-after:  _kdb5_util_load:
   :end-before: _kdb5_util_load_end:


Examples
########

To load a single principal, either replacing or updating the database:

::

     shell% kdb5_util load dumpfile principal
     shell%

     shell% kdb5_util load -update dumpfile principal
     shell%


.. note::

          If the database file exists, and the *-update* flag was not
          given, *kdb5_util* will overwrite the existing database.

Using kdb5_util to upgrade a master KDC from krb5 1.1.x:

::

    shell% kdb5_util dump old-kdb-dump
    shell% kdb5_util dump -ov old-kdb-dump.ov
      [Create a new KDC installation, using the old stash file/master password]
    shell% kdb5_util load old-kdb-dump
    shell% kdb5_util load -update old-kdb-dump.ov

The use of old-kdb-dump.ov for an extra dump and load is necessary
to preserve per-principal policy information, which is not included in
the default dump format of krb5 1.1.x.

.. note::

          Using kdb5_util to dump and reload the principal database is
          only necessary when upgrading from versions of krb5 prior
          to 1.2.0---newer versions will use the existing database as-is.


.. _create_stash:

Creating a stash file
~~~~~~~~~~~~~~~~~~~~~

A stash file allows a KDC to authenticate itself to the database
utilities, such as :ref:`kadmind(8)`, :ref:`krb5kdc(8)`, and
:ref:`kdb5_util(8)`.

To create a stash file, use the :ref:`kdb5_util(8)` **stash** command.

.. include:: admin_commands/kdb5_util.rst
   :start-after: _kdb5_util_stash:
   :end-before: _kdb5_util_stash_end:


Example
#######

    shell% kdb5_util stash
    kdb5_util: Cannot find/read stored master key while reading master key
    kdb5_util: Warning: proceeding without master key
    Enter KDC database master key:  <= Type the KDC database master password.
    shell%

If you do not specify a stash file, kdb5_util will stash the key in
the file specified in your :ref:`kdc.conf(5)` file.


Creating and destroying a Kerberos database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to create a new Kerberos database, use the
:ref:`kdb5_util(8)` **create** command.

.. include:: admin_commands/kdb5_util.rst
   :start-after: _kdb5_util_create:
   :end-before: _kdb5_util_create_end:

If you need to destroy the current Kerberos database, use the
:ref:`kdb5_util(8)` **destroy** command.

.. include:: admin_commands/kdb5_util.rst
   :start-after: _kdb5_util_destroy:
   :end-before: _kdb5_util_destroy_end:


Examples
########

::

    shell% kdb5_util -r ATHENA.MIT.EDU create -s
    Loading random data
    Initializing database '/usr/local/var/krb5kdc/principal' for realm 'ATHENA.MIT.EDU',
    master key name 'K/M@ATHENA.MIT.EDU'
    You will be prompted for the database Master Password.
    It is important that you NOT FORGET this password.
    Enter KDC database master key:  <= Type the master password.
    Re-enter KDC database master key to verify:  <= Type it again.
    shell%

    shell% kdb5_util -r ATHENA.MIT.EDU destroy
    Deleting KDC database stored in '/usr/local/var/krb5kdc/principal', are you sure?
    (type 'yes' to confirm)?  <= yes
    OK, deleting database '/usr/local/var/krb5kdc/principal'...
    ** Database '/usr/local/var/krb5kdc/principal' destroyed.
    shell%


Updating the master key
~~~~~~~~~~~~~~~~~~~~~~~

Starting with release 1.7, :ref:`kdb5_util(8)` allows the master key
to be changed using a rollover process, with minimal loss of
availability.  To roll over the master key, follow these steps:

#. On the master KDC, run ``kdb5_util list_mkeys`` to view the current
   master key version number (KVNO).  If you have never rolled over
   the master key before, this will likely be version 1::

    $ kdb5_util list_mkeys
    Master keys for Principal: K/M@KRBTEST.COM
    KVNO: 1, Enctype: des-cbc-crc, Active on: Wed Dec 31 19:00:00 EST 1969 *

#. On the master KDC, run ``kdb5_util use_mkey 1`` to ensure that a
   master key activation list is present in the database.  This step
   is unnecessary in release 1.11.4 or later, or if the database was
   initially created with release 1.7 or later.

#. On the master KDC, run ``kdb5_util add_mkey -s`` to create a new
   master key and write it to the stash file.  Enter a secure password
   when prompted.  If this is the first time you are changing the
   master key, the new key will have version 2.  The new master key
   will not be used until you make it active.

#. Propagate the database to all slave KDCs, either manually or by
   waiting until the next scheduled propagation.  If you do not have
   any slave KDCs, you can skip this and the next step.

#. On each slave KDC, run ``kdb5_util list_mkeys`` to verify that the
   new master key is present, and then ``kdb5_util stash`` to write
   the new master key to the slave KDC's stash file.

#. On the master KDC, run ``kdb5_util use_mkey 2`` to begin using the
   new master key.  Replace ``2`` with the version of the new master
   key, as appropriate.  You can optionally specify a date for the new
   master key to become active; by default, it will become active
   immediately.  Prior to release 1.12, :ref:`kadmind(8)` must be
   restarted for this change to take full effect.

#. On the master KDC, run ``kdb5_util update_princ_encryption``.  This
   command will iterate over the database and re-encrypt all keys in
   the new master key.  If the database is large and uses DB2, the
   master KDC will become unavailable while this command runs, but
   clients should fail over to slave KDCs (if any are present) during
   this time period.  In release 1.13 and later, you can instead run
   ``kdb5_util -x unlockiter update_princ_encryption`` to use unlocked
   iteration; this variant will take longer, but will keep the
   database available to the KDC and kadmind while it runs.

#. On the master KDC, run ``kdb5_util purge_mkeys`` to clean up the
   old master key.


.. _ops_on_ldap:

Operations on the LDAP database
-------------------------------

The :ref:`kdb5_ldap_util(8)` is the primary tool for administrating
the Kerberos LDAP database.  It allows an administrator to manage
realms, Kerberos services (KDC and Admin Server) and ticket policies.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_synopsis:
   :end-before: _kdb5_ldap_util_synopsis_end:

**OPTIONS**

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_options:
   :end-before: _kdb5_ldap_util_options_end:


.. _ldap_create_realm:

Creating a Kerberos realm
~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to create a new realm, use the :ref:`kdb5_ldap_util(8)`
**create** command as follows.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_create:
   :end-before: _kdb5_ldap_util_create_end:


.. _ldap_mod_realm:

Modifying a Kerberos realm
~~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to modify a realm, use the :ref:`kdb5_ldap_util(8)`
**modify** command as follows.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_modify:
   :end-before: _kdb5_ldap_util_modify_end:


Destroying a Kerberos realm
~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to destroy a Kerberos realm, use the
:ref:`kdb5_ldap_util(8)` **destroy** command as follows.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_destroy:
   :end-before: _kdb5_ldap_util_destroy_end:


Retrieving information about a Kerberos realm
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to display the attributes of a realm, use the
:ref:`kdb5_ldap_util(8)` **view** command as follows.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_view:
   :end-before: _kdb5_ldap_util_view_end:


Listing available Kerberos realms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you need to display the list of the realms, use the
:ref:`kdb5_ldap_util(8)` **list** command as follows.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_list:
   :end-before: _kdb5_ldap_util_list_end:


.. _stash_ldap:

Stashing service object's password
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The :ref:`kdb5_ldap_util(8)` **stashsrvpw** command allows an
administrator to store the password of service object in a file.  The
KDC and Administration server uses this password to authenticate to
the LDAP server.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_stashsrvpw:
   :end-before: _kdb5_ldap_util_stashsrvpw_end:


Ticket Policy operations
~~~~~~~~~~~~~~~~~~~~~~~~

Creating a Ticket Policy
########################

To create a new ticket policy in directory , use the
:ref:`kdb5_ldap_util(8)` **create_policy** command.  Ticket policy
objects are created under the realm container.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_create_policy:
   :end-before: _kdb5_ldap_util_create_policy_end:


Modifying a Ticket Policy
#########################

To modify a ticket policy in directory, use the
:ref:`kdb5_ldap_util(8)` **modify_policy** command.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_modify_policy:
   :end-before: _kdb5_ldap_util_modify_policy_end:


Retrieving Information About a Ticket Policy
############################################

To display the attributes of a ticket policy, use the
:ref:`kdb5_ldap_util(8)` **view_policy** command.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_view_policy:
   :end-before: _kdb5_ldap_util_view_policy_end:


Destroying a Ticket Policy
##########################

To destroy an existing ticket policy, use the :ref:`kdb5_ldap_util(8)`
**destroy_policy** command.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_destroy_policy:
   :end-before: _kdb5_ldap_util_destroy_policy_end:


Listing available Ticket Policies
#################################

To list the name of ticket policies in a realm, use the
:ref:`kdb5_ldap_util(8)` **list_policy** command.

.. include:: admin_commands/kdb5_ldap_util.rst
   :start-after:  _kdb5_ldap_util_list_policy:
   :end-before: _kdb5_ldap_util_list_policy_end:


.. _xrealm_authn:

Cross-realm authentication
--------------------------

In order for a KDC in one realm to authenticate Kerberos users in a
different realm, it must share a key with the KDC in the other realm.
In both databases, there must be krbtgt service principals for both realms.
For example, if you need to do cross-realm authentication between the realms
``ATHENA.MIT.EDU`` and ``EXAMPLE.COM``, you would need to add the
principals ``krbtgt/EXAMPLE.COM@ATHENA.MIT.EDU`` and
``krbtgt/ATHENA.MIT.EDU@EXAMPLE.COM`` to both databases.
These principals must all have the same passwords, key version
numbers, and encryption types; this may require explicitly setting
the key version number with the **-kvno** option.

In the ATHENA.MIT.EDU and EXAMPLE.COM cross-realm case, the administrators
would run the following commands on the KDCs in both realms::

    shell%: kadmin.local -e "aes256-cts:normal"
    kadmin: addprinc -requires_preauth krbtgt/ATHENA.MIT.EDU@EXAMPLE.COM
    Enter password for principal krbtgt/ATHENA.MIT.EDU@EXAMPLE.COM:
    Re-enter password for principal krbtgt/ATHENA.MIT.EDU@EXAMPLE.COM:
    kadmin: addprinc -requires_preauth krbtgt/EXAMPLE.COM@ATHENA.MIT.EDU
    Enter password for principal krbtgt/EXAMPLE.COM@ATHENA.MIT.EDU:
    Enter password for principal krbtgt/EXAMPLE.COM@ATHENA.MIT.EDU:
    kadmin:

.. note::

          Even if most principals in a realm are generally created
          with the **requires_preauth** flag enabled, this flag is not
          desirable on cross-realm authentication keys because doing
          so makes it impossible to disable preauthentication on a
          service-by-service basis.  Disabling it as in the example
          above is recommended.

.. note::

          It is very important that these principals have good
          passwords.  MIT recommends that TGT principal passwords be
          at least 26 characters of random ASCII text.


.. _changing_krbtgt_key:

Changing the krbtgt key
-----------------------

A Kerberos Ticket Granting Ticket (TGT) is a service ticket for the
principal ``krbtgt/REALM``.  The key for this principal is created
when the Kerberos database is initialized and need not be changed.
However, it will only have the encryption types supported by the KDC
at the time of the initial database creation.  To allow use of newer
encryption types for the TGT, this key has to be changed.

Changing this key using the normal :ref:`kadmin(1)`
**change_password** command would invalidate any previously issued
TGTs.  Therefore, when changing this key, normally one should use the
**-keepold** flag to change_password to retain the previous key in the
database as well as the new key.  For example::

    kadmin: change_password -randkey -keepold krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU

.. warning::

             After issuing this command, the old key is still valid
             and is still vulnerable to (for instance) brute force
             attacks.  To completely retire an old key or encryption
             type, run the kadmin **purgekeys** command to delete keys
             with older kvnos, ideally first making sure that all
             tickets issued with the old keys have expired.

Only the first krbtgt key of the newest key version is used to encrypt
ticket-granting tickets.  However, the set of encryption types present
in the krbtgt keys is used by default to determine the session key
types supported by the krbtgt service (see
:ref:`session_key_selection`).  Because non-MIT Kerberos clients
sometimes send a limited set of encryption types when making AS
requests, it can be important to for the krbtgt service to support
multiple encryption types.  This can be accomplished by giving the
krbtgt principal multiple keys, which is usually as simple as not
specifying any **-e** option when changing the krbtgt key, or by
setting the **session_enctypes** string attribute on the krbtgt
principal (see :ref:`set_string`).

Due to a bug in releases 1.8 through 1.13, renewed and forwarded
tickets may not work if the original ticket was obtained prior to a
krbtgt key change and the modified ticket is obtained afterwards.
Upgrading the KDC to release 1.14 or later will correct this bug.


.. _incr_db_prop:

Incremental database propagation
--------------------------------

Overview
~~~~~~~~

At some very large sites, dumping and transmitting the database can
take more time than is desirable for changes to propagate from the
master KDC to the slave KDCs.  The incremental propagation support
added in the 1.7 release is intended to address this.

With incremental propagation enabled, all programs on the master KDC
that change the database also write information about the changes to
an "update log" file, maintained as a circular buffer of a certain
size.  A process on each slave KDC connects to a service on the master
KDC (currently implemented in the :ref:`kadmind(8)` server) and
periodically requests the changes that have been made since the last
check.  By default, this check is done every two minutes.  If the
database has just been modified in the previous several seconds
(currently the threshold is hard-coded at 10 seconds), the slave will
not retrieve updates, but instead will pause and try again soon after.
This reduces the likelihood that incremental update queries will cause
delays for an administrator trying to make a bunch of changes to the
database at the same time.

Incremental propagation uses the following entries in the per-realm
data in the KDC config file (See :ref:`kdc.conf(5)`):

====================== =============== ===========================================
iprop_enable           *boolean*       If *true*, then incremental propagation is enabled, and (as noted below) normal kprop propagation is disabled. The default is *false*.
iprop_master_ulogsize  *integer*       Indicates the number of entries that should be retained in the update log. The default is 1000; the maximum number is 2500.
iprop_slave_poll       *time interval* Indicates how often the slave should poll the master KDC for changes to the database. The default is two minutes.
iprop_port             *integer*       Specifies the port number to be used for incremental propagation. This is required in both master and slave configuration files.
iprop_resync_timeout   *integer*       Specifies the number of seconds to wait for a full propagation to complete. This is optional on slave configurations.  Defaults to 300 seconds (5 minutes).
iprop_logfile          *file name*     Specifies where the update log file for the realm database is to be stored. The default is to use the *database_name* entry from the realms section of the config file :ref:`kdc.conf(5)`, with *.ulog* appended. (NOTE: If database_name isn't specified in the realms section, perhaps because the LDAP database back end is being used, or the file name is specified in the *dbmodules* section, then the hard-coded default for *database_name* is used. Determination of the *iprop_logfile*  default value will not use values from the *dbmodules* section.)
====================== =============== ===========================================

Both master and slave sides must have a principal named
``kiprop/hostname`` (where *hostname* is the lowercase,
fully-qualified, canonical name for the host) registered in the
Kerberos database, and have keys for that principal stored in the
default keytab file (|keytab|).  In release 1.13, the
``kiprop/hostname`` principal is created automatically for the master
KDC, but it must still be created for slave KDCs.

On the master KDC side, the ``kiprop/hostname`` principal must be
listed in the kadmind ACL file :ref:`kadm5.acl(5)`, and given the
**p** privilege (see :ref:`privileges`).

On the slave KDC side, :ref:`kpropd(8)` should be run.  When
incremental propagation is enabled, it will connect to the kadmind on
the master KDC and start requesting updates.

The normal kprop mechanism is disabled by the incremental propagation
support.  However, if the slave has been unable to fetch changes from
the master KDC for too long (network problems, perhaps), the log on
the master may wrap around and overwrite some of the updates that the
slave has not yet retrieved.  In this case, the slave will instruct
the master KDC to dump the current database out to a file and invoke a
one-time kprop propagation, with special options to also convey the
point in the update log at which the slave should resume fetching
incremental updates.  Thus, all the keytab and ACL setup previously
described for kprop propagation is still needed.

If an environment has a large number of slaves, it may be desirable to
arrange them in a hierarchy instead of having the master serve updates
to every slave.  To do this, run ``kadmind -proponly`` on each
intermediate slave, and ``kpropd -A upstreamhostname`` on downstream
slaves to direct each one to the appropriate upstream slave.

There are several known restrictions in the current implementation:

- The incremental update protocol does not transport changes to policy
  objects.  Any policy changes on the master will result in full
  resyncs to all slaves.
- The slave's KDB module must support locking; it cannot be using the
  LDAP KDB module.
- The master and slave must be able to initiate TCP connections in
  both directions, without an intervening NAT.


Sun/MIT incremental propagation differences
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sun donated the original code for supporting incremental database
propagation to MIT.  Some changes have been made in the MIT source
tree that will be visible to administrators.  (These notes are based
on Sun's patches.  Changes to Sun's implementation since then may not
be reflected here.)

The Sun config file support looks for ``sunw_dbprop_enable``,
``sunw_dbprop_master_ulogsize``, and ``sunw_dbprop_slave_poll``.

The incremental propagation service is implemented as an ONC RPC
service.  In the Sun implementation, the service is registered with
rpcbind (also known as portmapper) and the client looks up the port
number to contact.  In the MIT implementation, where interaction with
some modern versions of rpcbind doesn't always work well, the port
number must be specified in the config file on both the master and
slave sides.

The Sun implementation hard-codes pathnames in ``/var/krb5`` for the
update log and the per-slave kprop dump files.  In the MIT
implementation, the pathname for the update log is specified in the
config file, and the per-slave dump files are stored in
|kdcdir|\ ``/slave_datatrans_hostname``.
