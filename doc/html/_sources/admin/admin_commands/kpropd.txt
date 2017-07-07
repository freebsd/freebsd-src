.. _kpropd(8):

kpropd
======

SYNOPSIS
--------

**kpropd**
[**-r** *realm*]
[**-A** *admin_server*]
[**-a** *acl_file*]
[**-f** *slave_dumpfile*]
[**-F** *principal_database*]
[**-p** *kdb5_util_prog*]
[**-P** *port*]
[**-d**]
[**-t**]

DESCRIPTION
-----------

The *kpropd* command runs on the slave KDC server.  It listens for
update requests made by the :ref:`kprop(8)` program.  If incremental
propagation is enabled, it periodically requests incremental updates
from the master KDC.

When the slave receives a kprop request from the master, kpropd
accepts the dumped KDC database and places it in a file, and then runs
:ref:`kdb5_util(8)` to load the dumped database into the active
database which is used by :ref:`krb5kdc(8)`.  This allows the master
Kerberos server to use :ref:`kprop(8)` to propagate its database to
the slave servers.  Upon a successful download of the KDC database
file, the slave Kerberos server will have an up-to-date KDC database.

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
enabled, the slave periodically polls the master KDC for updates, at
an interval determined by the **iprop_slave_poll** variable.  If the
slave receives updates, kpropd updates its log file with any updates
from the master.  :ref:`kproplog(8)` can be used to view a summary of
the update entry log on the slave KDC.  If incremental propagation is
enabled, the principal ``kiprop/slavehostname@REALM`` (where
*slavehostname* is the name of the slave KDC host, and *REALM* is the
name of the Kerberos realm) must be present in the slave's keytab
file.

:ref:`kproplog(8)` can be used to force full replication when iprop is
enabled.


OPTIONS
--------

**-r** *realm*
    Specifies the realm of the master server.

**-A** *admin_server*
    Specifies the server to be contacted for incremental updates; by
    default, the master admin server is contacted.

**-f** *file*
    Specifies the filename where the dumped principal database file is
    to be stored; by default the dumped database file is |kdcdir|\
    ``/from_master``.

**-p**
    Allows the user to specify the pathname to the :ref:`kdb5_util(8)`
    program; by default the pathname used is |sbindir|\
    ``/kdb5_util``.

**-d**
    Turn on debug mode.  In this mode, kpropd will not detach
    itself from the current job and run in the background.  Instead,
    it will run in the foreground and print out debugging messages
    during the database propagation.

**-t**
    In standalone mode without incremental propagation, exit after one
    dump file is received.  In incremental propagation mode, exit as
    soon as the database is up to date, or if the master returns an
    error.

**-P**
    Allow for an alternate port number for kpropd to listen on.  This
    is only useful in combination with the **-S** option.

**-a** *acl_file*
    Allows the user to specify the path to the kpropd.acl file; by
    default the path used is |kdcdir|\ ``/kpropd.acl``.


ENVIRONMENT
-----------

kpropd uses the following environment variables:

* **KRB5_CONFIG**
* **KRB5_KDC_PROFILE**


FILES
-----

kpropd.acl
    Access file for kpropd; the default location is
    ``/usr/local/var/krb5kdc/kpropd.acl``.  Each entry is a line
    containing the principal of a host from which the local machine
    will allow Kerberos database propagation via :ref:`kprop(8)`.


SEE ALSO
--------

:ref:`kprop(8)`, :ref:`kdb5_util(8)`, :ref:`krb5kdc(8)`, inetd(8)
