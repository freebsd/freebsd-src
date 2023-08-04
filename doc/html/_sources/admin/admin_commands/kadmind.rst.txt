.. _kadmind(8):

kadmind
=======

SYNOPSIS
--------

**kadmind**
[**-x** *db_args*]
[**-r** *realm*]
[**-m**]
[**-nofork**]
[**-proponly**]
[**-port** *port-number*]
[**-P** *pid_file*]
[**-p** *kdb5_util_path*]
[**-K** *kprop_path*]
[**-k** *kprop_port*]
[**-F** *dump_file*]

DESCRIPTION
-----------

kadmind starts the Kerberos administration server.  kadmind typically
runs on the primary Kerberos server, which stores the KDC database.
If the KDC database uses the LDAP module, the administration server
and the KDC server need not run on the same machine.  kadmind accepts
remote requests from programs such as :ref:`kadmin(1)` and
:ref:`kpasswd(1)` to administer the information in these database.

kadmind requires a number of configuration files to be set up in order
for it to work:

:ref:`kdc.conf(5)`
    The KDC configuration file contains configuration information for
    the KDC and admin servers.  kadmind uses settings in this file to
    locate the Kerberos database, and is also affected by the
    **acl_file**, **dict_file**, **kadmind_port**, and iprop-related
    settings.

:ref:`kadm5.acl(5)`
    kadmind's ACL (access control list) tells it which principals are
    allowed to perform administration actions.  The pathname to the
    ACL file can be specified with the **acl_file** :ref:`kdc.conf(5)`
    variable; by default, it is |kdcdir|\ ``/kadm5.acl``.

After the server begins running, it puts itself in the background and
disassociates itself from its controlling terminal.

kadmind can be configured for incremental database propagation.
Incremental propagation allows replica KDC servers to receive
principal and policy updates incrementally instead of receiving full
dumps of the database.  This facility can be enabled in the
:ref:`kdc.conf(5)` file with the **iprop_enable** option.  Incremental
propagation requires the principal ``kiprop/PRIMARY\@REALM`` (where
PRIMARY is the primary KDC's canonical host name, and REALM the realm
name).  In release 1.13, this principal is automatically created and
registered into the datebase.


OPTIONS
-------

**-r** *realm*
    specifies the realm that kadmind will serve; if it is not
    specified, the default realm of the host is used.

**-m**
    causes the master database password to be fetched from the
    keyboard (before the server puts itself in the background, if not
    invoked with the **-nofork** option) rather than from a file on
    disk.

**-nofork**
    causes the server to remain in the foreground and remain
    associated to the terminal.

**-proponly**
    causes the server to only listen and respond to Kerberos replica
    incremental propagation polling requests.  This option can be used
    to set up a hierarchical propagation topology where a replica KDC
    provides incremental updates to other Kerberos replicas.

**-port** *port-number*
    specifies the port on which the administration server listens for
    connections.  The default port is determined by the
    **kadmind_port** configuration variable in :ref:`kdc.conf(5)`.

**-P** *pid_file*
    specifies the file to which the PID of kadmind process should be
    written after it starts up.  This file can be used to identify
    whether kadmind is still running and to allow init scripts to stop
    the correct process.

**-p** *kdb5_util_path*
    specifies the path to the kdb5_util command to use when dumping the
    KDB in response to full resync requests when iprop is enabled.

**-K** *kprop_path*
    specifies the path to the kprop command to use to send full dumps
    to replicas in response to full resync requests.

**-k** *kprop_port*
    specifies the port by which the kprop process that is spawned by
    kadmind connects to the replica kpropd, in order to transfer the
    dump file during an iprop full resync request.

**-F** *dump_file*
    specifies the file path to be used for dumping the KDB in response
    to full resync requests when iprop is enabled.

**-x** *db_args*
    specifies database-specific arguments.  See :ref:`Database Options
    <dboptions>` in :ref:`kadmin(1)` for supported arguments.


ENVIRONMENT
-----------

See :ref:`kerberos(7)` for a description of Kerberos environment
variables.


SEE ALSO
--------

:ref:`kpasswd(1)`, :ref:`kadmin(1)`, :ref:`kdb5_util(8)`,
:ref:`kdb5_ldap_util(8)`, :ref:`kadm5.acl(5)`, :ref:`kerberos(7)`
