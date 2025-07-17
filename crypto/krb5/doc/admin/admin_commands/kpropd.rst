.. _kpropd(8):

kpropd
======

SYNOPSIS
--------

**kpropd**
[**-r** *realm*]
[**-A** *admin_server*]
[**-a** *acl_file*]
[**-f** *replica_dumpfile*]
[**-F** *principal_database*]
[**-p** *kdb5_util_prog*]
[**-P** *port*]
[**--pid-file**\ =\ *pid_file*]
[**-D**]
[**-d**]
[**-s** *keytab_file*]

DESCRIPTION
-----------

The *kpropd* command runs on the replica KDC server.  It listens for
update requests made by the :ref:`kprop(8)` program.  If incremental
propagation is enabled, it periodically requests incremental updates
from the primary KDC.

When the replica receives a kprop request from the primary, kpropd
accepts the dumped KDC database and places it in a file, and then runs
:ref:`kdb5_util(8)` to load the dumped database into the active
database which is used by :ref:`krb5kdc(8)`.  This allows the primary
Kerberos server to use :ref:`kprop(8)` to propagate its database to
the replica servers.  Upon a successful download of the KDC database
file, the replica Kerberos server will have an up-to-date KDC
database.

Where incremental propagation is not used, kpropd is commonly invoked
out of inetd(8) as a nowait service.  This is done by adding a line to
the ``/etc/inetd.conf`` file which looks like this::

    kprop  stream  tcp  nowait  root  /usr/local/sbin/kpropd  kpropd

kpropd can also run as a standalone daemon, backgrounding itself and
waiting for connections on port 754 (or the port specified with the
**-P** option if given).  Standalone mode is required for incremental
propagation.  Starting in release 1.11, kpropd automatically detects
whether it was run from inetd and runs in standalone mode if it is
not.  Prior to release 1.11, the **-S** option is required to run
kpropd in standalone mode; this option is now accepted for backward
compatibility but does nothing.

Incremental propagation may be enabled with the **iprop_enable**
variable in :ref:`kdc.conf(5)`.  If incremental propagation is
enabled, the replica periodically polls the primary KDC for updates, at
an interval determined by the **iprop_replica_poll** variable.  If the
replica receives updates, kpropd updates its log file with any updates
from the primary.  :ref:`kproplog(8)` can be used to view a summary of
the update entry log on the replica KDC.  If incremental propagation
is enabled, the principal ``kiprop/replicahostname@REALM`` (where
*replicahostname* is the name of the replica KDC host, and *REALM* is
the name of the Kerberos realm) must be present in the replica's
keytab file.

:ref:`kproplog(8)` can be used to force full replication when iprop is
enabled.


OPTIONS
--------

**-r** *realm*
    Specifies the realm of the primary server.

**-A** *admin_server*
    Specifies the server to be contacted for incremental updates; by
    default, the primary admin server is contacted.

**-f** *file*
    Specifies the filename where the dumped principal database file is
    to be stored; by default the dumped database file is |kdcdir|\
    ``/from_master``.

**-F** *kerberos_db*
    Path to the Kerberos database file, if not the default.

**-p**
    Allows the user to specify the pathname to the :ref:`kdb5_util(8)`
    program; by default the pathname used is |sbindir|\
    ``/kdb5_util``.

**-D**
    In this mode, kpropd will not detach itself from the current job
    and run in the background.  Instead, it will run in the
    foreground.

**-d**
    Turn on debug mode.  kpropd will print out debugging messages
    during the database propogation and will run in the foreground
    (implies **-D**).

**-P**
    Allow for an alternate port number for kpropd to listen on.  This
    is only useful in combination with the **-S** option.

**-a** *acl_file*
    Allows the user to specify the path to the kpropd.acl file; by
    default the path used is |kdcdir|\ ``/kpropd.acl``.

**--pid-file**\ =\ *pid_file*
    In standalone mode, write the process ID of the daemon into
    *pid_file*.

**-s** *keytab_file*
    Path to a keytab to use for acquiring acceptor credentials.

**-x** *db_args*
    Database-specific arguments.  See :ref:`Database Options
    <dboptions>` in :ref:`kadmin(1)` for supported arguments.


FILES
-----

kpropd.acl
    Access file for kpropd; the default location is
    ``/usr/local/var/krb5kdc/kpropd.acl``.  Each entry is a line
    containing the principal of a host from which the local machine
    will allow Kerberos database propagation via :ref:`kprop(8)`.


ENVIRONMENT
-----------

See :ref:`kerberos(7)` for a description of Kerberos environment
variables.


SEE ALSO
--------

:ref:`kprop(8)`, :ref:`kdb5_util(8)`, :ref:`krb5kdc(8)`,
:ref:`kerberos(7)`, inetd(8)
