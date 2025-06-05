.. _dbtypes:

Database types
==============

A Kerberos database can be implemented with one of three built-in
database providers, called KDB modules.  Software which incorporates
the MIT krb5 KDC may also provide its own KDB module.  The following
subsections describe the three built-in KDB modules and the
configuration specific to them.

The database type can be configured with the **db_library** variable
in the :ref:`dbmodules` subsection for the realm.  For example::

    [dbmodules]
        ATHENA.MIT.EDU = {
            db_library = db2
        }

If the ``ATHENA.MIT.EDU`` realm subsection contains a
**database_module** setting, then the subsection within
``[dbmodules]`` should use that name instead of ``ATHENA.MIT.EDU``.

To transition from one database type to another, stop the
:ref:`kadmind(8)` service, use ``kdb5_util dump`` to create a dump
file, change the **db_library** value and set any appropriate
configuration for the new database type, and use ``kdb5_util load`` to
create and populate the new database.  If the new database type is
LDAP, create the new database using ``kdb5_ldap_util`` and populate it
from the dump file using ``kdb5_util load -update``.  Then restart the
:ref:`krb5kdc(8)` and :ref:`kadmind(8)` services.


Berkeley database module (db2)
------------------------------

The default KDB module is ``db2``, which uses a version of the
Berkeley DB library.  It creates four files based on the database
pathname.  If the pathname ends with ``principal`` then the four files
are:

* ``principal``, containing principal entry data
* ``principal.ok``, a lock file for the principal database
* ``principal.kadm5``, containing policy object data
* ``principal.kadm5.lock``, a lock file for the policy database

For large databases, the :ref:`kdb5_util(8)` **dump** command (perhaps
invoked by :ref:`kprop(8)` or by :ref:`kadmind(8)` for incremental
propagation) may cause :ref:`krb5kdc(8)` to stop for a noticeable
period of time while it iterates over the database.  This delay can be
avoided by disabling account lockout features so that the KDC does not
perform database writes (see :ref:`disable_lockout`).  Alternatively,
a slower form of iteration can be enabled by setting the
**unlockiter** variable to ``true``.  For example::

    [dbmodules]
        ATHENA.MIT.EDU = {
            db_library = db2
            unlockiter = true
        }

In rare cases, a power failure or other unclean system shutdown may
cause inconsistencies in the internal pointers within a database file,
such that ``kdb5_util dump`` cannot retrieve all principal entries in
the database.  In this situation, it may be possible to retrieve all
of the principal data by running ``kdb5_util dump -recurse`` to
iterate over the database using the tree pointers instead of the
iteration pointers.  Running ``kdb5_util dump -rev`` to iterate over
the database backwards may also retrieve some of the data which is not
retrieved by a normal dump operation.


Lightning Memory-Mapped Database module (klmdb)
-----------------------------------------------

The klmdb module was added in release 1.17.  It uses the LMDB library,
and may offer better performance and reliability than the db2 module.
It creates four files based on the database pathname.  If the pathname
ends with ``principal``, then the four files are:

* ``principal.mdb``, containing policy object data and most principal
  entry data
* ``principal.mdb-lock``, a lock file for the primary database
* ``principal.lockout.mdb``, containing the account lockout attributes
  (last successful authentication time, last failed authentication
  time, and number of failed attempts) for each principal entry
* ``principal.lockout.mdb-lock``, a lock file for the lockout database

Separating out the lockout attributes ensures that the KDC will never
block on an administrative operation such as a database dump or load.
It also allows the KDC to operate without write access to the primary
database.  If both account lockout features are disabled (see
:ref:`disable_lockout`), the lockout database files will be created
but will not subsequently be opened, and the account lockout
attributes will always have zero values.

Because LMDB creates a memory map to the database files, it requires a
configured memory map size which also determines the maximum size of
the database.  This size is applied equally to the two databases, so
twice the configured size will be consumed in the process address
space; this is primarily a limitation on 32-bit platforms.  The
default value of 128 megabytes should be sufficient for several
hundred thousand principal entries.  If the limit is reached, kadmin
operations will fail and the error message "Environment mapsize limit
reached" will appear in the kadmind log file.  In this case, the
**mapsize** variable can be used to increase the map size.  The
following example sets the map size to 512 megabytes::

    [dbmodules]
        ATHENA.MIT.EDU = {
            db_library = klmdb
            mapsize = 512
        }

LMDB has a configurable maximum number of readers.  The default value
of 128 should be sufficient for most deployments.  If you are going to
use a large number of KDC worker processes, it may be necessary to set
the **max_readers** variable to a larger number.

By default, LMDB synchronizes database files to disk after each write
transaction to ensure durability in the case of an unclean system
shutdown.  The klmdb module always turns synchronization off for the
lockout database to ensure reasonable KDC performance, but leaves it
on for the primary database.  If high throughput for administrative
operations (including password changes) is required, the **nosync**
variable can be set to "true" to disable synchronization for the
primary database.

The klmdb module does not support explicit locking with the
:ref:`kadmin(1)` **lock** command.


LDAP module (kldap)
-------------------

The kldap module stores principal and policy data using an LDAP
server.  To use it you must configure an LDAP server to use the
Kerberos schema.  See :ref:`conf_ldap` for details.

Because :ref:`krb5kdc(8)` is single-threaded, latency in LDAP database
accesses may limit KDC operation throughput.  If the LDAP server is
located on the same server host as the KDC and accessed through an
``ldapi://`` URL, latency should be minimal.  If this is not possible,
consider starting multiple KDC worker processes with the
:ref:`krb5kdc(8)` **-w** option to enable concurrent processing of KDC
requests.

The kldap module does not support explicit locking with the
:ref:`kadmin(1)` **lock** command.
