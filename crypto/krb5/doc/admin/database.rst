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
principal ``kadmin/admin`` or ``kadmin/HOST`` (where *HOST* is the
hostname of the admin server).  If the credentials cache contains a
ticket for either service principal and the **-c** ccache option is
specified, that ticket is used to authenticate to KADM5.  Otherwise,
the **-p** and **-k** options are used to specify the client Kerberos
principal name used to authenticate.  Once kadmin has determined the
principal name, it requests a ``kadmin/admin`` Kerberos service ticket
from the KDC, and uses that service ticket to authenticate to KADM5.

See :ref:`kadmin(1)` for the available kadmin and kadmin.local
commands and options.


.. _principals:

Principals
----------

Each entry in the Kerberos database contains a Kerberos principal and
the attributes and policies associated with that principal.

To add a principal to the database, use the :ref:`kadmin(1)`
**add_principal** command.  User principals should usually be created
with the ``+requires_preauth -allow_svr`` options to help mitigate
dictionary attacks (see :ref:`dictionary`)::

    kadmin: addprinc +requires_preauth -allow_svr alice
    Enter password for principal "alice@KRBTEST.COM":
    Re-enter password for principal "alice@KRBTEST.COM":

User principals which will authenticate with :ref:`pkinit` should
instead by created with the ``-nokey`` option:

    kadmin: addprinc -nokey alice

Service principals can be created with the ``-nokey`` option;
long-term keys will be added when a keytab is generated::

    kadmin: addprinc -nokey host/foo.mit.edu
    kadmin: ktadd -k foo.keytab host/foo.mit.edu
    Entry for principal host/foo.mit.edu with kvno 1, encryption type aes256-cts-hmac-sha1-96 added to keytab WRFILE:foo.keytab.
    Entry for principal host/foo.mit.edu with kvno 1, encryption type aes128-cts-hmac-sha1-96 added to keytab WRFILE:foo.keytab.

To modify attributes of an existing principal, use the kadmin
**modify_principal** command::

    kadmin: modprinc -expire tomorrow alice
    Principal "alice@KRBTEST.COM" modified.

To delete a principal, use the kadmin **delete_principal** command::

    kadmin: delprinc alice
    Are you sure you want to delete the principal "alice@KRBTEST.COM"? (yes/no): yes
    Principal "alice@KRBTEST.COM" deleted.
    Make sure that you have removed this principal from all ACLs before reusing.

To change a principal's password, use the kadmin **change_password**
command.  Password changes made through kadmin are subject to the same
password policies as would apply to password changes made through
:ref:`kpasswd(1)`.

To view the attributes of a principal, use the kadmin`
**get_principal** command.

To generate a listing of principals, use the kadmin
**list_principals** command.


.. _policies:

Policies
--------

A policy is a set of rules governing passwords.  Policies can dictate
minimum and maximum password lifetimes, minimum number of characters
and character classes a password must contain, and the number of old
passwords kept in the database.

To add a new policy, use the :ref:`kadmin(1)` **add_policy** command::

    kadmin: addpol -maxlife "1 year" -history 3 stduser

To modify attributes of a principal, use the kadmin **modify_policy**
command.  To delete a policy, use the kadmin **delete_policy**
command.

To associate a policy with a principal, use the kadmin
**modify_principal** command with the **-policy** option:

    kadmin: modprinc -policy stduser alice
    Principal "alice@KRBTEST.COM" modified.

A principal entry may be associated with a nonexistent policy, either
because the policy did not exist at the time of associated or was
deleted afterwards.  kadmin will warn when associated a principal with
a nonexistent policy, and will annotate the policy name with "[does
not exist]" in the **get_principal** output.


.. _updating_history_key:

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
the Kerberos database when using the DB2 or LMDB modules (see
:ref:`dbtypes`).  Creating a database is described in
:ref:`create_db`.

To create a stash file using the master password (because the database
was not created with one using the ``create -s`` flag, or after
restoring from a backup which did not contain the stash file), use the
kdb5_util **stash** command::

    $ kdb5_util stash
    kdb5_util: Cannot find/read stored master key while reading master key
    kdb5_util: Warning: proceeding without master key
    Enter KDC database master key:  <= Type the KDC database master password.

To destroy a database, use the kdb5_util destroy command::

    $ kdb5_util destroy
    Deleting KDC database stored in '/var/krb5kdc/principal', are you sure?
    (type 'yes' to confirm)? yes
    OK, deleting database '/var/krb5kdc/principal'...
    ** Database '/var/krb5kdc/principal' destroyed.


.. _restore_from_dump:

Dumping and loading a Kerberos database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To dump a Kerberos database into a text file for backup or transfer
purposes, use the :ref:`kdb5_util(8)` **dump** command on one of the
KDCs::

    $ kdb5_util dump dumpfile

    $ kbd5_util dump -verbose dumpfile
    kadmin/admin@ATHENA.MIT.EDU
    krbtgt/ATHENA.MIT.EDU@ATHENA.MIT.EDU
    kadmin/history@ATHENA.MIT.EDU
    K/M@ATHENA.MIT.EDU
    kadmin/changepw@ATHENA.MIT.EDU

You may specify which principals to dump, using full principal names
including realm::

    $ kdb5_util dump -verbose someprincs K/M@ATHENA.MIT.EDU kadmin/admin@ATHENA.MIT.EDU
    kadmin/admin@ATHENA.MIT.EDU
    K/M@ATHENA.MIT.EDU

To restore a Kerberos database dump from a file, use the
:ref:`kdb5_util(8)` **load** command::

    $ kdb5_util load dumpfile

To update an existing database with a partial dump file containing
only some principals, use the ``-update`` flag::

    $ kdb5_util load -update someprincs

.. note::

          If the database file exists, and the *-update* flag was not
          given, *kdb5_util* will overwrite the existing database.


.. _updating_master_key:

Updating the master key
~~~~~~~~~~~~~~~~~~~~~~~

Starting with release 1.7, :ref:`kdb5_util(8)` allows the master key
to be changed using a rollover process, with minimal loss of
availability.  To roll over the master key, follow these steps:

#. On the primary KDC, run ``kdb5_util list_mkeys`` to view the
   current master key version number (KVNO).  If you have never rolled
   over the master key before, this will likely be version 1::

    $ kdb5_util list_mkeys
    Master keys for Principal: K/M@KRBTEST.COM
    KVNO: 1, Enctype: aes256-cts-hmac-sha384-192, Active on: Thu Jan 01 00:00:00 UTC 1970 *

#. On the primary KDC, run ``kdb5_util use_mkey 1`` to ensure that a
   master key activation list is present in the database.  This step
   is unnecessary in release 1.11.4 or later, or if the database was
   initially created with release 1.7 or later.

#. On the primary KDC, run ``kdb5_util add_mkey -s`` to create a new
   master key and write it to the stash file.  Enter a secure password
   when prompted.  If this is the first time you are changing the
   master key, the new key will have version 2.  The new master key
   will not be used until you make it active.

#. Propagate the database to all replica KDCs, either manually or by
   waiting until the next scheduled propagation.  If you do not have
   any replica KDCs, you can skip this and the next step.

#. On each replica KDC, run ``kdb5_util list_mkeys`` to verify that
   the new master key is present, and then ``kdb5_util stash`` to
   write the new master key to the replica KDC's stash file.

#. On the primary KDC, run ``kdb5_util use_mkey 2`` to begin using the
   new master key.  Replace ``2`` with the version of the new master
   key, as appropriate.  You can optionally specify a date for the new
   master key to become active; by default, it will become active
   immediately.  Prior to release 1.12, :ref:`kadmind(8)` must be
   restarted for this change to take full effect.

#. On the primary KDC, run ``kdb5_util update_princ_encryption``.
   This command will iterate over the database and re-encrypt all keys
   in the new master key.  If the database is large and uses DB2, the
   primary KDC will become unavailable while this command runs, but
   clients should fail over to replica KDCs (if any are present)
   during this time period.  In release 1.13 and later, you can
   instead run ``kdb5_util -x unlockiter update_princ_encryption`` to
   use unlocked iteration; this variant will take longer, but will
   keep the database available to the KDC and kadmind while it runs.

#. Wait until the above changes have propagated to all replica KDCs
   and until all running KDC and kadmind processes have serviced
   requests using updated principal entries.

#. On the primary KDC, run ``kdb5_util purge_mkeys`` to clean up the
   old master key.


.. _ops_on_ldap:

Operations on the LDAP database
-------------------------------

The :ref:`kdb5_ldap_util(8)` command is the primary tool for
administrating the Kerberos database when using the LDAP module.
Creating an LDAP Kerberos database is describe in :ref:`conf_ldap`.

To view a list of realms in the LDAP database, use the kdb5_ldap_util
**list** command::

    $ kdb5_ldap_util list
    KRBTEST.COM

To modify the attributes of a realm, use the kdb5_ldap_util **modify**
command.  For example, to change the default realm's maximum ticket
life::

    $ kdb5_ldap_util modify -maxtktlife "10 hours"

To display the attributes of a realm, use the kdb5_ldap_util **view**
command::

    $ kdb5_ldap_util view
                   Realm Name: KRBTEST.COM
          Maximum Ticket Life: 0 days 00:10:00

To remove a realm from the LDAP database, destroying its contents, use
the kdb5_ldap_util **destroy** command::

    $ kdb5_ldap_util destroy
    Deleting KDC database of 'KRBTEST.COM', are you sure?
    (type 'yes' to confirm)? yes
    OK, deleting database of 'KRBTEST.COM'...
    ** Database of 'KRBTEST.COM' destroyed.


Ticket Policy operations
~~~~~~~~~~~~~~~~~~~~~~~~

Unlike the DB2 and LMDB modules, the LDAP module supports ticket
policy objects, which can be associated with principals to restrict
maximum ticket lifetimes and set mandatory principal flags.  Ticket
policy objects are distinct from the password policies described
earlier on this page, and are chiefly managed through kdb5_ldap_util
rather than kadmin.  To create a new ticket policy, use the
kdb5_ldap_util **create_policy** command::

    $ kdb5_ldap_util create_policy -maxrenewlife "2 days" users

To associate a ticket policy with a principal, use the
:ref:`kadmin(1)` **modify_principal** (or **add_principal**) command
with the **-x tktpolicy=**\ *policy* option::

    $ kadmin.local modprinc -x tktpolicy=users alice

To remove a ticket policy reference from a principal, use the same
command with an empty *policy*::

    $ kadmin.local modprinc -x tktpolicy= alice

To list the existing ticket policy objects, use the kdb5_ldap_util
**list_policy** command::

    $ kdb5_ldap_util list_policy
    users

To modify the attributes of a ticket policy object, use the
kdb5_ldap_util **modify_policy** command::

    $ kdb5_ldap_util modify_policy -allow_svr +requires_preauth users

To view the attributes of a ticket policy object, use the
kdb5_ldap_util **view_policy** command::

    $ kdb5_ldap_util view_policy users
                Ticket policy: users
       Maximum renewable life: 2 days 00:00:00
                 Ticket flags: REQUIRES_PRE_AUTH DISALLOW_SVR

To destroy an ticket policy object, use the kdb5_ldap_util
**destroy_policy** command::

    $ kdb5_ldap_util destroy_policy users
    This will delete the policy object 'users', are you sure?
    (type 'yes' to confirm)? yes
    ** policy object 'users' deleted.


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
requests, it can be important for the krbtgt service to support
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
primary KDC to the replica KDCs.  The incremental propagation support
added in the 1.7 release is intended to address this.

With incremental propagation enabled, all programs on the primary KDC
that change the database also write information about the changes to
an "update log" file, maintained as a circular buffer of a certain
size.  A process on each replica KDC connects to a service on the
primary KDC (currently implemented in the :ref:`kadmind(8)` server) and
periodically requests the changes that have been made since the last
check.  By default, this check is done every two minutes.

Incremental propagation uses the following entries in the per-realm
data in the KDC config file (See :ref:`kdc.conf(5)`):

====================== =============== ===========================================
iprop_enable           *boolean*       If *true*, then incremental propagation is enabled, and (as noted below) normal kprop propagation is disabled. The default is *false*.
iprop_master_ulogsize  *integer*       Indicates the number of entries that should be retained in the update log. The default is 1000; the maximum number is 2500.
iprop_replica_poll     *time interval* Indicates how often the replica should poll the primary KDC for changes to the database. The default is two minutes.
iprop_port             *integer*       Specifies the port number to be used for incremental propagation. This is required in both primary and replica configuration files.
iprop_resync_timeout   *integer*       Specifies the number of seconds to wait for a full propagation to complete. This is optional on replica configurations.  Defaults to 300 seconds (5 minutes).
iprop_logfile          *file name*     Specifies where the update log file for the realm database is to be stored. The default is to use the *database_name* entry from the realms section of the config file :ref:`kdc.conf(5)`, with *.ulog* appended. (NOTE: If database_name isn't specified in the realms section, perhaps because the LDAP database back end is being used, or the file name is specified in the *dbmodules* section, then the hard-coded default for *database_name* is used. Determination of the *iprop_logfile*  default value will not use values from the *dbmodules* section.)
====================== =============== ===========================================

Both primary and replica sides must have a principal named
``kiprop/hostname`` (where *hostname* is the lowercase,
fully-qualified, canonical name for the host) registered in the
Kerberos database, and have keys for that principal stored in the
default keytab file (|keytab|).  The ``kiprop/hostname`` principal may
have been created automatically for the primary KDC, but it must
always be created for replica KDCs.

On the primary KDC side, the ``kiprop/hostname`` principal must be
listed in the kadmind ACL file :ref:`kadm5.acl(5)`, and given the
**p** privilege (see :ref:`privileges`).

On the replica KDC side, :ref:`kpropd(8)` should be run.  When
incremental propagation is enabled, it will connect to the kadmind on
the primary KDC and start requesting updates.

The normal kprop mechanism is disabled by the incremental propagation
support.  However, if the replica has been unable to fetch changes
from the primary KDC for too long (network problems, perhaps), the log
on the primary may wrap around and overwrite some of the updates that
the replica has not yet retrieved.  In this case, the replica will
instruct the primary KDC to dump the current database out to a file
and invoke a one-time kprop propagation, with special options to also
convey the point in the update log at which the replica should resume
fetching incremental updates.  Thus, all the keytab and ACL setup
previously described for kprop propagation is still needed.

If an environment has a large number of replicas, it may be desirable
to arrange them in a hierarchy instead of having the primary serve
updates to every replica.  To do this, run ``kadmind -proponly`` on
each intermediate replica, and ``kpropd -A upstreamhostname`` on
downstream replicas to direct each one to the appropriate upstream
replica.

There are several known restrictions in the current implementation:

- The incremental update protocol does not transport changes to policy
  objects.  Any policy changes on the primary will result in full
  resyncs to all replicas.
- The replica's KDB module must support locking; it cannot be using the
  LDAP KDB module.
- The primary and replica must be able to initiate TCP connections in
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
number must be specified in the config file on both the primary and
replica sides.

The Sun implementation hard-codes pathnames in ``/var/krb5`` for the
update log and the per-replica kprop dump files.  In the MIT
implementation, the pathname for the update log is specified in the
config file, and the per-replica dump files are stored in
|kdcdir|\ ``/replica_datatrans_hostname``.
